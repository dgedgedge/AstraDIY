#include "astralim_step_pwm_actor.h"

#include <cmath>
#include <stdexcept>

namespace AstrAlim
{

AstraStepPwmActor::AstraStepPwmActor(int gpioValue, double stepPercentValue, const std::string& name)
    : AstraComActor(name.empty() ? ("AstraStepPwmActor(GPIO" + std::to_string(gpioValue) + ")") : name),
      gpio(gpioValue),
      stepPercent(stepPercentValue)
{
    openGpioLine(gpio);
    updateHighStepCount();
}

AstraStepPwmActor::~AstraStepPwmActor()
{
    close();
}

void AstraStepPwmActor::openGpioLine(int gpioValue)
{
    if (gpioValue != 13 && gpioValue != 18)
        throw std::runtime_error("Unsupported GPIO. Only GPIO 13 and GPIO 18 are allowed");

    if (!gpioController.openChip())
        throw std::runtime_error("Unable to open gpio chip");

    if (gpioController.isLineUsed(gpioValue))
    {
        const std::string consumer = gpioController.getLineConsumer(gpioValue);
        if (!consumer.empty())
            throw std::runtime_error("GPIO already used by: " + consumer);
        throw std::runtime_error("GPIO already used");
    }

    if (!gpioController.requestOutput(gpioValue, "AstraStepPwmActor", 0))
        throw std::runtime_error("Unable to request GPIO output");
}

void AstraStepPwmActor::updateHighStepCount()
{
    const int stepCount = cycleStepCount > 0 ? cycleStepCount : 1;
    int computed = static_cast<int>(std::lround(static_cast<double>(stepCount) * stepPercent / 100.0));
    computed = std::max(0, std::min(computed, stepCount));
    highStepCount = computed;

    highStepIndexes.clear();
    if (highStepCount == 0)
        return;

    if (highStepCount >= stepCount)
    {
        for (int i = 0; i < stepCount; ++i)
            highStepIndexes.insert(i);
        return;
    }

    for (int i = 0; i < highStepCount; ++i)
    {
        const int distributed = static_cast<int>(std::lround(static_cast<double>(i) * static_cast<double>(stepCount) /
                                                             static_cast<double>(highStepCount))) % stepCount;
        highStepIndexes.insert(distributed);
    }

    int candidate = 0;
    while (static_cast<int>(highStepIndexes.size()) < highStepCount && candidate < stepCount)
    {
        highStepIndexes.insert(candidate);
        candidate += 1;
    }
}

void AstraStepPwmActor::setStepPercent(double stepPercentValue)
{
    if (stepPercentValue < 0.0 || stepPercentValue > 100.0)
        throw std::runtime_error("stepPercent must be between 0 and 100");

    stepPercent = stepPercentValue;
    updateHighStepCount();
}

double AstraStepPwmActor::getStepPercent() const
{
    return stepPercent;
}

double AstraStepPwmActor::getPwmUpdatePeriodS() const
{
    return cyclePeriodS > 0.0 ? cyclePeriodS : 0.0;
}

bool AstraStepPwmActor::isHigh()
{
    return gpioController.getValue(gpio) != 0;
}

void AstraStepPwmActor::beforeMeasurements(int step)
{
    gpioController.setValue(gpio, highStepIndexes.count(step) > 0 ? 1 : 0);
}

void AstraStepPwmActor::onCycleConfigurationChanged(double periodS, int stepCount)
{
    AstraComActor::onCycleConfigurationChanged(periodS, stepCount);
    updateHighStepCount();
}

void AstraStepPwmActor::close()
{
    gpioController.setValue(gpio, 0);
    gpioController.releaseLine(gpio);
    gpioController.closeChip();
}

} // namespace AstrAlim