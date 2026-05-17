#ifndef ASTRALIM_ASTRA_INA_H
#define ASTRALIM_ASTRA_INA_H

#include "astralim_com_device.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace AstrAlim
{

class AstraIna : public AstraComDevice
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

    struct Info
    {
        std::string name;
        int address = -1;
        bool pingOk = false;
        double voltageV = 0.0;
        double shuntVoltagemV = 0.0;
        double currentmA = 0.0;
        double powermW = 0.0;
        double energiemWS = 0.0;
        double intPeriodS = 0.0;
    };

    struct SensorConfig
    {
        bool isPwm = false;
        int busNum = 1;
        int address = 0x40;
        double shuntOhms = 0.01;
        double maxExpectedAmps = 6.0;
        bool forceAbsCurrentPower = false;
        AdcResolution busAdc = ADC_12BIT;
        AdcResolution shuntAdc = ADC_12BIT;
    };

    static const std::map<std::string, SensorConfig>& sensorSet();
    static std::vector<std::string> getListNames();
    static void exitAll();

    // Constructor using predefined sensor name (e.g., "AstraPwm1", "AstraDc1")
    explicit AstraIna(const std::string& name, bool autoRegisterToFetcher = true);

    // Constructor using individual sensor characteristics
    AstraIna(double shuntOhms,
             double maxExpectedAmps,
             int busNum,
             int address,
             const std::string& name = "AstraIna",
             bool autoRegisterToFetcher = true);

    ~AstraIna();

    void startMeasurement(int step, double integrationDurationS) override;
    void getMeasurement(int step, double integrationDurationS) override;
    void onCycleConfigurationChanged(double periodS, int stepCount) override;

    bool getPingOK() const;

    void configure(VoltageRange voltageRange = RANGE_16V,
                   int gain = GAIN_AUTO,
                   AdcResolution busAdc = ADC_12BIT,
                   AdcResolution shuntAdc = ADC_12BIT);

    double voltageV() const;
    double shuntVoltagemV() const;
    double shuntVoltageV() const;
    double currentmA() const;
    double currentA() const;
    double powermW() const;
    double powerW() const;
    double energiemWS() const;
    double energieWS() const;
    double intPeriodS() const;
    Info getInfo() const;

private:
    struct Impl;

    static constexpr int MIN_VALID_SAMPLES_ABS = 3;
    static constexpr double MIN_VALID_SAMPLES_RATIO = 0.40;
    static constexpr double PUBLISH_SMOOTH_ALPHA = 0.35;

    void initializeFromConfig(const SensorConfig& cfg);
    void resetCycleAccumulators();
    void publishCurrentCycleAverage();
    void accumulateCycleAndPublish(int step);

    bool eachStep = false;
    bool isPwm = false;
    bool configured = false;
    bool configurationSent = false;
    bool pingOk = true;

    int address = -1;
    VoltageRange voltageRange = RANGE_16V;
    int gain = GAIN_AUTO;
    AdcResolution busAdc = ADC_12BIT;
    AdcResolution shuntAdc = ADC_12BIT;
    bool forceAbsCurrentPower = false;

    double integrationPeriodS = 0.0;
    double voltageValueV = 0.0;
    double shuntVoltagemVValue = 0.0;
    double currentValuemA = 0.0;
    double powerValuemW = 0.0;
    double energymWS = 0.0;

    double publishedVoltageV = 0.0;
    double publishedShuntVoltagemV = 0.0;
    double publishedCurrentmA = 0.0;
    double publishedPowermW = 0.0;

    double cycleVoltageSumV = 0.0;
    double cycleShuntVoltageSummV = 0.0;
    double cycleCurrentSummA = 0.0;
    double cyclePowerSummW = 0.0;
    int cycleSampleCount = 0;

    std::unique_ptr<Impl> impl;
};

} // namespace AstrAlim

#endif // ASTRALIM_ASTRA_INA_H