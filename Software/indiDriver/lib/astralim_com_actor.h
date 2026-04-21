#ifndef ASTRALIM_COM_ACTOR_H
#define ASTRALIM_COM_ACTOR_H

#include <string>

namespace AstrAlim
{

class AstraComFetcher;

class AstraComActor
{
public:
    explicit AstraComActor(std::string name = "");
    virtual ~AstraComActor();

    virtual void beforeMeasurements(int step);

    const std::string& getName() const;
    void setName(const std::string& nameValue);

    virtual void onCycleConfigurationChanged(double periodS, int stepCount);

protected:
    AstraComFetcher* registeredFetcher = nullptr;
    double cyclePeriodS = 0.0;
    int cycleStepCount = 0;

private:
    std::string name;
};

} // namespace AstrAlim

#endif // ASTRALIM_COM_ACTOR_H