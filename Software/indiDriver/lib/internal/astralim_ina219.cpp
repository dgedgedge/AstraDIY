#include "internal/astralim_ina219.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace AstrAlim
{

std::mutex Ina219::busMapMutex;
std::map<int, std::shared_ptr<std::mutex>> Ina219::busLocks;

Ina219::Ina219(double shuntOhmsValue, double maxExpectedAmpsValue, int busNumValue, int addressValue)
    : busNum(busNumValue),
      address(addressValue),
      shuntOhms(shuntOhmsValue),
      maxExpectedAmps(maxExpectedAmpsValue)
{
    if (shuntOhms <= 0.0)
        throw std::runtime_error("INA219: invalid shunt resistance");

    {
        std::lock_guard<std::mutex> lock(busMapMutex);
        auto it = busLocks.find(busNum);
        if (it == busLocks.end())
        {
            busLock = std::make_shared<std::mutex>();
            busLocks[busNum] = busLock;
        }
        else
        {
            busLock = it->second;
        }
    }

    minDeviceCurrentLsb = calculateMinCurrentLsb();
    ensureOpen();
}

Ina219::~Ina219()
{
    closeFd();
}

void Ina219::ensureOpen()
{
    if (fd >= 0)
        return;

    const std::string devPath = "/dev/i2c-" + std::to_string(busNum);
    fd = open(devPath.c_str(), O_RDWR);
    if (fd < 0)
    {
        throw std::runtime_error("INA219: cannot open " + devPath + ": " + std::strerror(errno));
    }

    if (ioctl(fd, I2C_SLAVE, address) < 0)
    {
        closeFd();
        throw std::runtime_error("INA219: cannot select I2C address 0x" + std::to_string(address));
    }
}

void Ina219::closeFd()
{
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
}

bool Ina219::ping()
{
    try
    {
        (void) readRegisterU16(REG_BUS_VOLTAGE);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Ina219::configure(VoltageRange vrange, int gainValue, AdcResolution busAdc, AdcResolution shuntAdc)
{
    if (vrange != RANGE_16V && vrange != RANGE_32V)
        throw std::runtime_error("INA219: invalid voltage range");

    voltageRange = vrange;

    if (gainValue == GAIN_AUTO)
    {
        autoGainEnabled = true;
        gain = determineGain(maxExpectedAmps > 0.0 ? maxExpectedAmps : 0.1);
    }
    else
    {
        autoGainEnabled = false;
        gain = gainValue;
    }

    calibrate(rangeToVoltage(voltageRange), gainToVoltage(gain), maxExpectedAmps > 0.0 ? maxExpectedAmps : 0.0);

    const uint16_t config =
        static_cast<uint16_t>((voltageRange << BIT_BRNG) |
                              (gain << BIT_PG0) |
                              (busAdc << BIT_BADC1) |
                              (shuntAdc << BIT_SADC1) |
                              MODE_CONT_SHUNT_BUS);
    writeConfiguration(config);
}

double Ina219::voltage()
{
    const int regValue = voltageRegisterValue();
    return (static_cast<double>(regValue) * BUS_MILLIVOLTS_LSB) / 1000.0;
}

double Ina219::currentMilliAmps()
{
    handleCurrentOverflow();
    const int16_t reg = readRegisterS16(REG_CURRENT);
    return static_cast<double>(reg) * currentLsb * 1000.0;
}

double Ina219::powerMilliWatts()
{
    handleCurrentOverflow();
    const uint16_t reg = readRegisterU16(REG_POWER);
    return static_cast<double>(reg) * powerLsb * 1000.0;
}

double Ina219::shuntMilliVolts()
{
    handleCurrentOverflow();
    const int16_t reg = readRegisterS16(REG_SHUNT_VOLTAGE);
    return static_cast<double>(reg) * SHUNT_MILLIVOLTS_LSB;
}

bool Ina219::currentOverflow()
{
    return hasCurrentOverflow();
}

void Ina219::reset()
{
    writeConfiguration(static_cast<uint16_t>(1U << BIT_RST));
}

void Ina219::handleCurrentOverflow()
{
    if (autoGainEnabled)
    {
        while (hasCurrentOverflow())
        {
            increaseGain();
        }
    }
    else if (hasCurrentOverflow())
    {
        throw std::runtime_error("INA219: current overflow");
    }
}

void Ina219::increaseGain()
{
    const int currentGain = readGain();
    if (currentGain >= GAIN_8_320MV)
        throw std::runtime_error("INA219: gain limit reached");

    const int nextGain = currentGain + 1;
    calibrate(rangeToVoltage(voltageRange), gainToVoltage(nextGain), maxExpectedAmps > 0.0 ? maxExpectedAmps : 0.0);
    configureGain(nextGain);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

int Ina219::determineGain(double expectedAmps) const
{
    const double shuntV = expectedAmps * shuntOhms;
    if (shuntV > gainToVoltage(GAIN_8_320MV))
        throw std::runtime_error("INA219: expected current out of gain range");

    for (int g = GAIN_1_40MV; g <= GAIN_8_320MV; g++)
    {
        if (gainToVoltage(g) > shuntV)
            return g;
    }
    return GAIN_8_320MV;
}

double Ina219::determineCurrentLsb(double expectedAmps, double maxPossibleAmps) const
{
    double lsb = maxPossibleAmps / CURRENT_LSB_FACTOR;

    if (expectedAmps > 0.0)
    {
        if (expectedAmps > maxPossibleAmps)
            throw std::runtime_error("INA219: expected current larger than max possible current");
        lsb = std::min(expectedAmps, maxPossibleAmps) / CURRENT_LSB_FACTOR;
    }

    return std::max(lsb, minDeviceCurrentLsb);
}

void Ina219::calibrate(double busVoltsMax, double shuntVoltsMax, double expectedAmps)
{
    (void) busVoltsMax;
    const double maxPossibleAmps = shuntVoltsMax / shuntOhms;

    currentLsb = determineCurrentLsb(expectedAmps, maxPossibleAmps);
    powerLsb = currentLsb * 20.0;

    const double calibration = std::trunc(CALIBRATION_FACTOR / (currentLsb * shuntOhms));
    const uint16_t calibrationValue = static_cast<uint16_t>(
        std::max(1.0, std::min(calibration, static_cast<double>(MAX_CALIBRATION_VALUE))));

    writeRegister(REG_CALIBRATION, calibrationValue);
}

void Ina219::configureGain(int newGain)
{
    uint16_t config = readConfiguration();
    config = static_cast<uint16_t>(config & 0xE7FF);
    config = static_cast<uint16_t>(config | (newGain << BIT_PG0));
    writeConfiguration(config);
    gain = newGain;
}

int Ina219::readGain() const
{
    const uint16_t config = readConfiguration();
    return static_cast<int>((config & 0x1800) >> BIT_PG0);
}

uint16_t Ina219::readConfiguration() const
{
    return const_cast<Ina219*>(this)->readRegisterU16(REG_CONFIG);
}

void Ina219::writeConfiguration(uint16_t value)
{
    writeRegister(REG_CONFIG, value);
}

bool Ina219::hasCurrentOverflow() const
{
    const uint16_t reg = const_cast<Ina219*>(this)->readRegisterU16(REG_BUS_VOLTAGE);
    return (reg & MASK_OVF) == MASK_OVF;
}

int Ina219::voltageRegisterValue() const
{
    const uint16_t reg = const_cast<Ina219*>(this)->readRegisterU16(REG_BUS_VOLTAGE);
    return static_cast<int>(reg >> 3);
}

double Ina219::calculateMinCurrentLsb() const
{
    return CALIBRATION_FACTOR / (shuntOhms * static_cast<double>(MAX_CALIBRATION_VALUE));
}

double Ina219::gainToVoltage(int gainValue)
{
    static const std::array<double, 4> gainVolts {0.04, 0.08, 0.16, 0.32};
    if (gainValue < 0 || gainValue >= static_cast<int>(gainVolts.size()))
        return gainVolts.back();
    return gainVolts[static_cast<size_t>(gainValue)];
}

double Ina219::rangeToVoltage(int rangeValue)
{
    return (rangeValue == RANGE_16V) ? 16.0 : 32.0;
}

void Ina219::writeRegister(uint8_t reg, uint16_t value)
{
    ensureOpen();
    const uint8_t buffer[3] = {reg, static_cast<uint8_t>((value >> 8) & 0xFF), static_cast<uint8_t>(value & 0xFF)};

    std::lock_guard<std::mutex> lock(*busLock);
    for (int retry = 0; retry < 4; retry++)
    {
        if (write(fd, buffer, sizeof(buffer)) == static_cast<ssize_t>(sizeof(buffer)))
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    throw std::runtime_error("INA219: write register failed");
}

uint16_t Ina219::readRegisterU16(uint8_t reg)
{
    ensureOpen();

    std::lock_guard<std::mutex> lock(*busLock);
    for (int retry = 0; retry < 4; retry++)
    {
        if (write(fd, &reg, 1) == 1)
        {
            uint8_t data[2] = {0, 0};
            if (read(fd, data, 2) == 2)
            {
                return static_cast<uint16_t>((data[0] << 8) | data[1]);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    throw std::runtime_error("INA219: read register failed");
}

int16_t Ina219::readRegisterS16(uint8_t reg)
{
    return static_cast<int16_t>(readRegisterU16(reg));
}

} // namespace AstrAlim
