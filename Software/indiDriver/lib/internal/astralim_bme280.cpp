#include "astralim_bme280.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <map>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace AstrAlim
{

std::mutex Bme280::busMapMutex;
std::map<int, std::shared_ptr<std::mutex>> Bme280::busLocks;

namespace
{
std::unique_ptr<Bme280> defaultSensor;
std::mutex defaultSensorMutex;

Bme280& getDefaultSensor()
{
    std::lock_guard<std::mutex> lock(defaultSensorMutex);
    if (!defaultSensor)
    {
        defaultSensor = std::make_unique<Bme280>();
    }
    return *defaultSensor;
}

int clampInt(int value, int minValue, int maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}
} // namespace

Bme280::Bme280(int addrValue, int busIdValue)
    : addr(addrValue), busId(busIdValue)
{
    std::lock_guard<std::mutex> lock(busMapMutex);
    auto it = busLocks.find(busId);
    if (it == busLocks.end())
    {
        busLock = std::make_shared<std::mutex>();
        busLocks[busId] = busLock;
    }
    else
    {
        busLock = it->second;
    }

    openBus();
}

Bme280::~Bme280()
{
    closeBus();
}

void Bme280::openBus()
{
    if (fd >= 0)
        return;

    const std::string devPath = "/dev/i2c-" + std::to_string(busId);
    fd = open(devPath.c_str(), O_RDWR);
    if (fd < 0)
    {
        throw std::runtime_error("BME280: cannot open " + devPath + ": " + std::strerror(errno));
    }
}

void Bme280::closeBus()
{
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
}

int Bme280::effectiveAddr(int requestedAddr) const
{
    return (requestedAddr < 0) ? addr : requestedAddr;
}

uint8_t Bme280::readByteData(int deviceAddr, uint8_t reg)
{
    auto values = readI2cBlockData(deviceAddr, reg, 1);
    return values[0];
}

void Bme280::writeByteData(int deviceAddr, uint8_t reg, uint8_t value)
{
    std::lock_guard<std::mutex> lock(*busLock);

    if (ioctl(fd, I2C_SLAVE, deviceAddr) < 0)
        throw std::runtime_error("BME280: ioctl I2C_SLAVE failed");

    uint8_t frame[2] = {reg, value};
    for (int retry = 0; retry < 4; retry++)
    {
        if (write(fd, frame, sizeof(frame)) == static_cast<ssize_t>(sizeof(frame)))
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    throw std::runtime_error("BME280: writeByteData failed");
}

std::vector<uint8_t> Bme280::readI2cBlockData(int deviceAddr, uint8_t reg, size_t len)
{
    std::lock_guard<std::mutex> lock(*busLock);

    if (ioctl(fd, I2C_SLAVE, deviceAddr) < 0)
        throw std::runtime_error("BME280: ioctl I2C_SLAVE failed");

    std::vector<uint8_t> data(len, 0);
    for (int retry = 0; retry < 4; retry++)
    {
        if (write(fd, &reg, 1) == 1)
        {
            if (read(fd, data.data(), static_cast<unsigned int>(len)) == static_cast<ssize_t>(len))
                return data;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    throw std::runtime_error("BME280: readI2cBlockData failed");
}

int Bme280::getShort(const std::vector<uint8_t>& data, size_t index) const
{
    const uint16_t value = static_cast<uint16_t>((data[index + 1] << 8) | data[index]);
    return static_cast<int>(static_cast<int16_t>(value));
}

int Bme280::getUShort(const std::vector<uint8_t>& data, size_t index) const
{
    return static_cast<int>((data[index + 1] << 8) | data[index]);
}

int Bme280::getChar(const std::vector<uint8_t>& data, size_t index) const
{
    return static_cast<int>(static_cast<int8_t>(data[index]));
}

int Bme280::getUChar(const std::vector<uint8_t>& data, size_t index) const
{
    return static_cast<int>(data[index] & 0xFF);
}

std::pair<int, int> Bme280::readBME280ID(int requestedAddr)
{
    const int deviceAddr = effectiveAddr(requestedAddr);
    auto id = readI2cBlockData(deviceAddr, 0xD0, 2);
    return {id[0], id[1]};
}

void Bme280::loadCalibrationData(int requestedAddr)
{
    const int deviceAddr = effectiveAddr(requestedAddr);

    auto cal1 = readI2cBlockData(deviceAddr, 0x88, 24);
    auto cal2 = readI2cBlockData(deviceAddr, 0xA1, 1);
    auto cal3 = readI2cBlockData(deviceAddr, 0xE1, 7);

    digT1 = getUShort(cal1, 0);
    digT2 = getShort(cal1, 2);
    digT3 = getShort(cal1, 4);

    digP1 = getUShort(cal1, 6);
    digP2 = getShort(cal1, 8);
    digP3 = getShort(cal1, 10);
    digP4 = getShort(cal1, 12);
    digP5 = getShort(cal1, 14);
    digP6 = getShort(cal1, 16);
    digP7 = getShort(cal1, 18);
    digP8 = getShort(cal1, 20);
    digP9 = getShort(cal1, 22);

    digH1 = getUChar(cal2, 0);
    digH2 = getShort(cal3, 0);
    digH3 = getUChar(cal3, 2);

    int h4 = getChar(cal3, 3);
    h4 = (h4 << 24) >> 20;
    digH4 = h4 | (getChar(cal3, 4) & 0x0F);

    int h5 = getChar(cal3, 5);
    h5 = (h5 << 24) >> 20;
    digH5 = h5 | ((getUChar(cal3, 4) >> 4) & 0x0F);

    digH6 = getChar(cal3, 6);

    calibrationLoaded = true;
}

void Bme280::configureNormalMode(int requestedAddr,
                                 int oversampleTemp,
                                 int oversamplePres,
                                 int oversampleHum,
                                 int standbyCode,
                                 int filterCode)
{
    const int deviceAddr = effectiveAddr(requestedAddr);

    oversampleTemp = clampInt(oversampleTemp, 0, 5);
    oversamplePres = clampInt(oversamplePres, 0, 5);
    oversampleHum = clampInt(oversampleHum, 0, 5);
    standbyCode = clampInt(standbyCode, 0, 7);
    filterCode = clampInt(filterCode, 0, 4);

    constexpr uint8_t regControlHum = 0xF2;
    constexpr uint8_t regConfig = 0xF5;
    constexpr uint8_t regControlMeas = 0xF4;
    constexpr uint8_t normalMode = 0x03;

    writeByteData(deviceAddr, regControlHum, static_cast<uint8_t>(oversampleHum));

    const uint8_t config = static_cast<uint8_t>((standbyCode << 5) | (filterCode << 2));
    writeByteData(deviceAddr, regConfig, config);

    const uint8_t control = static_cast<uint8_t>((oversampleTemp << 5) | (oversamplePres << 2) | normalMode);
    writeByteData(deviceAddr, regControlMeas, control);

    const double waitTimeMs =
        1.25 +
        (2.3 * static_cast<double>(oversampleTemp)) +
        ((2.3 * static_cast<double>(oversamplePres)) + 0.575) +
        ((2.3 * static_cast<double>(oversampleHum)) + 0.575);

    normalModeWaitTimeS = waitTimeMs / 1000.0;
    normalModeConfigured = true;

    std::this_thread::sleep_for(std::chrono::duration<double>(normalModeWaitTimeS));
}

std::tuple<double, double, double> Bme280::acquireDataNormalMode(int requestedAddr)
{
    const int deviceAddr = effectiveAddr(requestedAddr);

    if (!calibrationLoaded)
        loadCalibrationData(deviceAddr);

    if (!normalModeConfigured)
        configureNormalMode(deviceAddr);

    auto data = readI2cBlockData(deviceAddr, 0xF7, 8);

    const int presRaw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    const int tempRaw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    const int humRaw = (data[6] << 8) | data[7];

    const int var1 = ((((tempRaw >> 3) - (digT1 << 1))) * digT2) >> 11;
    const int var2 = (((((tempRaw >> 4) - digT1) * ((tempRaw >> 4) - digT1)) >> 12) * digT3) >> 14;
    const int tFine = var1 + var2;
    const double temperature = static_cast<double>(((tFine * 5) + 128) >> 8);

    double var1p = static_cast<double>(tFine) / 2.0 - 64000.0;
    double var2p = var1p * var1p * static_cast<double>(digP6) / 32768.0;
    var2p = var2p + var1p * static_cast<double>(digP5) * 2.0;
    var2p = var2p / 4.0 + static_cast<double>(digP4) * 65536.0;
    var1p = (static_cast<double>(digP3) * var1p * var1p / 524288.0 + static_cast<double>(digP2) * var1p) / 524288.0;
    var1p = (1.0 + var1p / 32768.0) * static_cast<double>(digP1);

    double pressure = 0.0;
    if (var1p != 0.0)
    {
        pressure = 1048576.0 - static_cast<double>(presRaw);
        pressure = ((pressure - var2p / 4096.0) * 6250.0) / var1p;
        const double var1p2 = static_cast<double>(digP9) * pressure * pressure / 2147483648.0;
        const double var2p2 = pressure * static_cast<double>(digP8) / 32768.0;
        pressure = pressure + (var1p2 + var2p2 + static_cast<double>(digP7)) / 16.0;
    }

    double humidity = static_cast<double>(tFine) - 76800.0;
    humidity =
        (static_cast<double>(humRaw) - (static_cast<double>(digH4) * 64.0 + static_cast<double>(digH5) / 16384.0 * humidity)) *
        (static_cast<double>(digH2) / 65536.0 *
         (1.0 + static_cast<double>(digH6) / 67108864.0 * humidity * (1.0 + static_cast<double>(digH3) / 67108864.0 * humidity)));
    humidity = humidity * (1.0 - static_cast<double>(digH1) * humidity / 524288.0);

    if (humidity > 100.0)
        humidity = 100.0;
    else if (humidity < 0.0)
        humidity = 0.0;

    return {temperature / 100.0, pressure / 100.0, humidity};
}

std::tuple<double, double, double> Bme280::readBME280All(int requestedAddr)
{
    return acquireDataNormalMode(requestedAddr);
}

int getShort(const std::vector<uint8_t>& data, size_t index)
{
    return getDefaultSensor().getShort(data, index);
}

int getUShort(const std::vector<uint8_t>& data, size_t index)
{
    return getDefaultSensor().getUShort(data, index);
}

int getChar(const std::vector<uint8_t>& data, size_t index)
{
    return getDefaultSensor().getChar(data, index);
}

int getUChar(const std::vector<uint8_t>& data, size_t index)
{
    return getDefaultSensor().getUChar(data, index);
}

std::pair<int, int> readBME280ID(int address)
{
    return getDefaultSensor().readBME280ID(address);
}

std::tuple<double, double, double> readBME280All(int address)
{
    return getDefaultSensor().readBME280All(address);
}

} // namespace AstrAlim
