#ifndef ASTRALIM_STEP_PWM_ACTOR_H
#define ASTRALIM_STEP_PWM_ACTOR_H

#include "astralim_com_actor.h"
#include "astralim_gpio.h"

#include <set>

namespace AstrAlim
{

class AstraStepPwmActor : public AstraComActor
{
public:
    explicit AstraStepPwmActor(int gpio, double stepPercent = 0.0, const std::string& name = "");
    ~AstraStepPwmActor() override;

    void beforeMeasurements(int step) override;
    void onCycleConfigurationChanged(double periodS, int stepCount) override;

    void setStepPercent(double stepPercent);
    double getStepPercent() const;
    double getPwmUpdatePeriodS() const;
    bool isHigh();

    void close();

private:
    void openGpioLine(int gpio);
    void updateHighStepCount();

    int gpio = -1;
    double stepPercent = 0.0;
    int highStepCount = 0;
    std::set<int> highStepIndexes;
    GpioController gpioController;
};

} // namespace AstrAlim

#endif // ASTRALIM_STEP_PWM_ACTOR_H