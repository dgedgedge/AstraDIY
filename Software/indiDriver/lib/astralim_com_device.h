#ifndef ASTRALIM_COM_DEVICE_H
#define ASTRALIM_COM_DEVICE_H

#include <string>

namespace AstrAlim
{

class AstraComFetcher;

class AstraComDevice
{
public:
    explicit AstraComDevice(bool eachStep = false, std::string name = "", bool autoRegisterToFetcher = true);
    virtual ~AstraComDevice() = default;

    void registerToFetcher(AstraComFetcher* fetcher = nullptr, bool eachStep = false);

    virtual void startMeasurement(int step, double integrationDurationS);
    virtual void getMeasurement(int step, double integrationDurationS) = 0;

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

#endif // ASTRALIM_COM_DEVICE_H