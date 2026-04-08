#include "astralim_com_actor.h"

#include "astralim_com_fetcher.h"

#include <utility>

namespace AstrAlim
{

AstraComActor::AstraComActor(std::string nameValue)
    : name(nameValue.empty() ? "AstraComActor" : std::move(nameValue))
{
    AstraComFetcher& fetcher = AstraComFetcher::getInstance();
    cyclePeriodS = fetcher.getCyclePeriod();
    cycleStepCount = fetcher.getCycleStepCount();
    fetcher.addActor(*this);
    registeredFetcher = &fetcher;
}

void AstraComActor::beforeMeasurements(int)
{
}

const std::string& AstraComActor::getName() const
{
    return name;
}

void AstraComActor::setName(const std::string& nameValue)
{
    name = nameValue;
}

void AstraComActor::onCycleConfigurationChanged(double periodS, int stepCount)
{
    cyclePeriodS = periodS;
    cycleStepCount = stepCount;
}

} // namespace AstrAlim