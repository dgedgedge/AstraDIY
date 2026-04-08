#include "astralim_com_device.h"

#include "astralim_com_fetcher.h"

#include <utility>

namespace AstrAlim
{

AstraComDevice::AstraComDevice(bool eachStep, std::string nameValue)
    : name(nameValue.empty() ? "AstraComDevice" : std::move(nameValue))
{
    registerToFetcher(nullptr, eachStep);
}

void AstraComDevice::registerToFetcher(AstraComFetcher* fetcher, bool eachStep)
{
    if (registeredFetcher != nullptr)
        return;

    AstraComFetcher& effectiveFetcher = (fetcher == nullptr) ? AstraComFetcher::getInstance() : *fetcher;

    cyclePeriodS = effectiveFetcher.getCyclePeriod();
    cycleStepCount = effectiveFetcher.getCycleStepCount();
    effectiveFetcher.addDevice(*this, eachStep);
    registeredFetcher = &effectiveFetcher;
}

void AstraComDevice::startMeasurement(int, double)
{
}

const std::string& AstraComDevice::getName() const
{
    return name;
}

void AstraComDevice::setName(const std::string& nameValue)
{
    name = nameValue;
}

void AstraComDevice::onCycleConfigurationChanged(double periodS, int stepCount)
{
    cyclePeriodS = periodS;
    cycleStepCount = stepCount;
}

} // namespace AstrAlim