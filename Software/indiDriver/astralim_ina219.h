#ifndef ASTRALIM_INA219_H
#define ASTRALIM_INA219_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace AstrAlim
{

class Ina219
{
public:
    enum VoltageRange
    {
        RANGE_16V = 0,
        RANGE_32V = 1,
    };

    enum Gain
    {
        GAIN_1_40MV = 0,
        GAIN_2_80MV = 1,
        GAIN_4_160MV = 2,
        GAIN_8_320MV = 3,
        GAIN_AUTO = -1,
    };

    enum AdcResolution
    {
        ADC_9BIT = 0,
        ADC_10BIT = 1,
        ADC_11BIT = 2,
        ADC_12BIT = 3,
        ADC_2SAMP = 9,
        ADC_4SAMP = 10,
        ADC_8SAMP = 11,
        ADC_16SAMP = 12,
        ADC_32SAMP = 13,
        ADC_64SAMP = 14,
        ADC_128SAMP = 15,
    };

    Ina219(double shuntOhms, double maxExpectedAmps, int busNum, int address);
    ~Ina219();

    bool ping();

    void configure(VoltageRange voltageRange = RANGE_32V,
                   int gain = GAIN_AUTO,
                   AdcResolution busAdc = ADC_12BIT,
                   AdcResolution shuntAdc = ADC_12BIT);

    double voltage();            // Volts
    double currentMilliAmps();  // mA
    double powerMilliWatts();   // mW
    double shuntMilliVolts();   // mV

    bool currentOverflow();
    void reset();

private:
    static constexpr uint8_t REG_CONFIG = 0x00;
    static constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;
    static constexpr uint8_t REG_BUS_VOLTAGE = 0x02;
    static constexpr uint8_t REG_POWER = 0x03;
    static constexpr uint8_t REG_CURRENT = 0x04;
    static constexpr uint8_t REG_CALIBRATION = 0x05;

    static constexpr uint16_t BIT_RST = 15;
    static constexpr uint16_t BIT_BRNG = 13;
    static constexpr uint16_t BIT_PG0 = 11;
    static constexpr uint16_t BIT_BADC1 = 7;
    static constexpr uint16_t BIT_SADC1 = 3;

    static constexpr uint16_t MODE_CONT_SHUNT_BUS = 7;

    static constexpr uint16_t MASK_OVF = 0x0001;

    static constexpr double SHUNT_MILLIVOLTS_LSB = 0.01; // 10uV
    static constexpr double BUS_MILLIVOLTS_LSB = 4.0;    // 4mV
    static constexpr double CALIBRATION_FACTOR = 0.04096;
    static constexpr int MAX_CALIBRATION_VALUE = 0xFFFE;
    static constexpr double CURRENT_LSB_FACTOR = 32800.0;

    static std::mutex busMapMutex;
    static std::map<int, std::shared_ptr<std::mutex>> busLocks;

    std::shared_ptr<std::mutex> busLock;
    int fd = -1;
    int busNum = 1;
    int address = 0x40;

    double shuntOhms = 0.01;
    double maxExpectedAmps = 0.0;

    int voltageRange = RANGE_32V;
    int gain = GAIN_1_40MV;
    bool autoGainEnabled = false;

    double minDeviceCurrentLsb = 0.0;
    double currentLsb = 0.0;
    double powerLsb = 0.0;

    void ensureOpen();
    void closeFd();

    void writeRegister(uint8_t reg, uint16_t value);
    uint16_t readRegisterU16(uint8_t reg);
    int16_t readRegisterS16(uint8_t reg);

    void handleCurrentOverflow();
    void increaseGain();
    int determineGain(double expectedAmps) const;
    double determineCurrentLsb(double expectedAmps, double maxPossibleAmps) const;

    void calibrate(double busVoltsMax, double shuntVoltsMax, double expectedAmps);
    void configureGain(int newGain);
    int readGain() const;
    uint16_t readConfiguration() const;
    void writeConfiguration(uint16_t value);

    bool hasCurrentOverflow() const;
    int voltageRegisterValue() const;
    double calculateMinCurrentLsb() const;

    static double gainToVoltage(int gainValue);
    static double rangeToVoltage(int rangeValue);
};

} // namespace AstrAlim

#endif // ASTRALIM_INA219_H
