#ifndef ASTRALIM_ASTRA_INA_H
#define ASTRALIM_ASTRA_INA_H

#include "astralim_com_device.h"
#include "astralim_ina219.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace AstrAlim
{

class AstraIna : public AstraComDevice
{
public:
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
        Ina219::AdcResolution busAdc = Ina219::ADC_12BIT;
        Ina219::AdcResolution shuntAdc = Ina219::ADC_12BIT;
    };

    static const std::map<std::string, SensorConfig>& sensorSet();
    static std::vector<std::string> getListNames();
    static void exitAll();

    AstraIna(double shuntOhms = -1.0,
             double maxExpectedAmps = -1.0,
             int busNum = -1,
             int address = -1,
             const std::string& name = "",
             bool autoRegisterToFetcher = true);

    void startMeasurement(int step, double integrationDurationS) override;
    void getMeasurement(int step, double integrationDurationS) override;
    void onCycleConfigurationChanged(double periodS, int stepCount) override;

    bool getPingOK() const;
    bool readSample(double integrationDurationS, double& outVoltageV, double& outCurrentA, double& outPowerW);

    void configure(Ina219::VoltageRange voltageRange = Ina219::RANGE_16V,
                   int gain = Ina219::GAIN_AUTO,
                   Ina219::AdcResolution busAdc = Ina219::ADC_12BIT,
                   Ina219::AdcResolution shuntAdc = Ina219::ADC_12BIT);

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
    static constexpr int MIN_VALID_SAMPLES_ABS = 3;
    static constexpr double MIN_VALID_SAMPLES_RATIO = 0.40;
    static constexpr double PUBLISH_SMOOTH_ALPHA = 0.35;

    void resetCycleAccumulators();
    void publishCurrentCycleAverage();
    void accumulateCycleAndPublish(int step);

    bool eachStep = false;
    bool isPwm = false;
    bool configured = false;
    bool configurationSent = false;
    bool pingOk = true;

    int address = -1;
    Ina219::VoltageRange voltageRange = Ina219::RANGE_16V;
    int gain = Ina219::GAIN_AUTO;
    Ina219::AdcResolution busAdc = Ina219::ADC_12BIT;
    Ina219::AdcResolution shuntAdc = Ina219::ADC_12BIT;
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

    std::unique_ptr<Ina219> ina;
};

} // namespace AstrAlim

#endif // ASTRALIM_ASTRA_INA_H