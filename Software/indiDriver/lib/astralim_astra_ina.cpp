#include "astralim_astra_ina.h"

#include "astralim_com_fetcher.h"
#include "internal/astralim_ina219.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace AstrAlim
{

struct AstraIna::Impl
{
    std::unique_ptr<Ina219> ina;
};

namespace
{
const std::map<std::string, AstraIna::SensorConfig> kSensorSet {
    {"AstraDc1", {false, 1, 0x41, 0.01, 6.0, true, AstraIna::ADC_12BIT, AstraIna::ADC_12BIT}},
    {"AstraDc2", {false, 1, 0x44, 0.01, 6.0, true, AstraIna::ADC_12BIT, AstraIna::ADC_12BIT}},
    {"AstraDc3", {false, 1, 0x46, 0.01, 6.0, true, AstraIna::ADC_12BIT, AstraIna::ADC_12BIT}},
    {"AstraPwm2", {true, 1, 0x49, 0.01, 6.0, false, AstraIna::ADC_12BIT, AstraIna::ADC_12BIT}},
    {"AstraPwm1", {true, 1, 0x4d, 0.01, 6.0, false, AstraIna::ADC_12BIT, AstraIna::ADC_12BIT}},
    {"AstOnStep", {false, 1, 0x40, 0.005, 6.0, false, AstraIna::ADC_12BIT, AstraIna::ADC_12BIT}},
};

Ina219::VoltageRange toInaVoltageRange(AstraIna::VoltageRange range)
{
    return static_cast<Ina219::VoltageRange>(range);
}

Ina219::AdcResolution toInaAdcResolution(AstraIna::AdcResolution adc)
{
    return static_cast<Ina219::AdcResolution>(adc);
}
}

const std::map<std::string, AstraIna::SensorConfig>& AstraIna::sensorSet()
{
    return kSensorSet;
}

std::vector<std::string> AstraIna::getListNames()
{
    std::vector<std::string> names;
    names.reserve(kSensorSet.size());
    for (const auto& item : kSensorSet)
        names.push_back(item.first);
    return names;
}

void AstraIna::exitAll()
{
    AstraComFetcher::exitAll();
}

AstraIna::AstraIna(const std::string& name, bool autoRegisterToFetcher)
        : AstraComDevice(false, name, false),
            impl(std::make_unique<Impl>())
{
    const auto it = kSensorSet.find(name);
    if (it == kSensorSet.end())
        throw std::runtime_error("Unknown AstraIna profile: " + name);

    initializeFromConfig(it->second);

    if (autoRegisterToFetcher)
        registerToFetcher(nullptr, it->second.isPwm);
}

AstraIna::AstraIna(double shuntOhms,
                   double maxExpectedAmps,
                   int busNum,
                   int addressValue,
                   const std::string& name,
                   bool autoRegisterToFetcher)
        : AstraComDevice(false, name, false),
            impl(std::make_unique<Impl>())
{
    if (shuntOhms <= 0.0 || maxExpectedAmps <= 0.0 || busNum < 0 || addressValue < 0)
        throw std::runtime_error("Invalid sensor parameters: shuntOhms/maxExpectedAmps/busNum/address must be positive");

    SensorConfig cfg;
    cfg.shuntOhms = shuntOhms;
    cfg.maxExpectedAmps = maxExpectedAmps;
    cfg.busNum = busNum;
    cfg.address = addressValue;

    initializeFromConfig(cfg);

    if (autoRegisterToFetcher)
        registerToFetcher(nullptr, cfg.isPwm);
}

AstraIna::~AstraIna() = default;

void AstraIna::initializeFromConfig(const SensorConfig& cfg)
{
    eachStep = cfg.isPwm;
    isPwm = cfg.isPwm;
    forceAbsCurrentPower = cfg.forceAbsCurrentPower;

    address = cfg.address;
    impl->ina = std::make_unique<Ina219>(cfg.shuntOhms, cfg.maxExpectedAmps, cfg.busNum, cfg.address);

    configure(RANGE_16V, GAIN_AUTO, cfg.busAdc, cfg.shuntAdc);
}

void AstraIna::startMeasurement(int, double)
{
    configurationSent = false;
    try
    {
        if (!impl || !impl->ina || !impl->ina->ping())
        {
            pingOk = false;
            return;
        }

        impl->ina->configure(toInaVoltageRange(voltageRange), gain, toInaAdcResolution(busAdc), toInaAdcResolution(shuntAdc));
        configurationSent = true;
        pingOk = true;
    }
    catch (...)
    {
        pingOk = false;
        configurationSent = false;
    }
}

bool AstraIna::getPingOK() const
{
    return pingOk;
}

void AstraIna::getMeasurement(int step, double integrationDurationS)
{
    if (!configurationSent || !impl || !impl->ina)
        return;

    try
    {
        if (!impl->ina->currentOverflow())
        {
            const double rawShuntmV = impl->ina->shuntMilliVolts();
            const double rawVoltageV = impl->ina->voltage();
            const double rawCurrentmA = impl->ina->currentMilliAmps();
            const double rawPowermW = impl->ina->powerMilliWatts();

            currentValuemA = forceAbsCurrentPower ? std::fabs(rawCurrentmA) : rawCurrentmA;
            powerValuemW = forceAbsCurrentPower ? std::fabs(rawPowermW) : rawPowermW;
            shuntVoltagemVValue = rawShuntmV;
            voltageValueV = std::max(rawVoltageV, 0.0);
            accumulateCycleAndPublish(step);
        }

        energymWS += powerValuemW * integrationDurationS;
        integrationPeriodS += integrationDurationS;
        pingOk = true;
    }
    catch (...)
    {
        pingOk = false;
    }
}

void AstraIna::resetCycleAccumulators()
{
    cycleVoltageSumV = 0.0;
    cycleShuntVoltageSummV = 0.0;
    cycleCurrentSummA = 0.0;
    cyclePowerSummW = 0.0;
    cycleSampleCount = 0;
}

void AstraIna::publishCurrentCycleAverage()
{
    if (cycleSampleCount <= 0)
        return;

    if (eachStep)
    {
        const int cycleSteps = cycleStepCount > 0 ? cycleStepCount : 10;
        const int minSamples = std::max(MIN_VALID_SAMPLES_ABS,
                                        static_cast<int>(std::lround(static_cast<double>(cycleSteps) * MIN_VALID_SAMPLES_RATIO)));
        if (cycleSampleCount < minSamples)
            return;
    }

    const double avgVoltageV = cycleVoltageSumV / static_cast<double>(cycleSampleCount);
    const double avgShuntmV = cycleShuntVoltageSummV / static_cast<double>(cycleSampleCount);
    const double avgCurrentmA = cycleCurrentSummA / static_cast<double>(cycleSampleCount);
    const double avgPowermW = cyclePowerSummW / static_cast<double>(cycleSampleCount);

    if (eachStep && (std::fabs(publishedVoltageV) > 0.0 || std::fabs(publishedCurrentmA) > 0.0))
    {
        const double alpha = PUBLISH_SMOOTH_ALPHA;
        publishedVoltageV = alpha * avgVoltageV + (1.0 - alpha) * publishedVoltageV;
        publishedShuntVoltagemV = alpha * avgShuntmV + (1.0 - alpha) * publishedShuntVoltagemV;
        publishedCurrentmA = alpha * avgCurrentmA + (1.0 - alpha) * publishedCurrentmA;
        publishedPowermW = alpha * avgPowermW + (1.0 - alpha) * publishedPowermW;
    }
    else
    {
        publishedVoltageV = avgVoltageV;
        publishedShuntVoltagemV = avgShuntmV;
        publishedCurrentmA = avgCurrentmA;
        publishedPowermW = avgPowermW;
    }
}

void AstraIna::accumulateCycleAndPublish(int step)
{
    if (!eachStep)
    {
        publishedVoltageV = voltageValueV;
        publishedShuntVoltagemV = shuntVoltagemVValue;
        publishedCurrentmA = currentValuemA;
        publishedPowermW = powerValuemW;
        return;
    }

    if (step == 0 && cycleSampleCount > 0)
    {
        publishCurrentCycleAverage();
        resetCycleAccumulators();
    }

    cycleVoltageSumV += voltageValueV;
    cycleShuntVoltageSummV += shuntVoltagemVValue;
    cycleCurrentSummA += currentValuemA;
    cyclePowerSummW += powerValuemW;
    cycleSampleCount += 1;
}

void AstraIna::onCycleConfigurationChanged(double periodS, int stepCount)
{
    AstraComDevice::onCycleConfigurationChanged(periodS, stepCount);
    resetCycleAccumulators();
}

void AstraIna::configure(VoltageRange voltageRangeValue,
                         int gainValue,
                         AdcResolution busAdcValue,
                         AdcResolution shuntAdcValue)
{
    if (configured)
        throw std::runtime_error("AstraIna already configured");

    voltageRange = voltageRangeValue;
    gain = gainValue;
    busAdc = busAdcValue;
    shuntAdc = shuntAdcValue;
    configured = true;
}

double AstraIna::voltageV() const { return publishedVoltageV; }
double AstraIna::shuntVoltagemV() const { return publishedShuntVoltagemV; }
double AstraIna::shuntVoltageV() const { return shuntVoltagemV() / 1000.0; }
double AstraIna::currentmA() const { return publishedCurrentmA; }
double AstraIna::currentA() const { return currentmA() / 1000.0; }
double AstraIna::powermW() const { return publishedPowermW; }
double AstraIna::powerW() const { return powermW() / 1000.0; }
double AstraIna::energiemWS() const { return energymWS; }
double AstraIna::energieWS() const { return energymWS / 1000.0; }
double AstraIna::intPeriodS() const { return integrationPeriodS; }

AstraIna::Info AstraIna::getInfo() const
{
    Info info;
    info.name = getName();
    info.address = address;
    info.pingOk = pingOk;
    info.voltageV = publishedVoltageV;
    info.shuntVoltagemV = publishedShuntVoltagemV;
    info.currentmA = publishedCurrentmA;
    info.powermW = publishedPowermW;
    info.energiemWS = energymWS;
    info.intPeriodS = integrationPeriodS;
    return info;
}

} // namespace AstrAlim