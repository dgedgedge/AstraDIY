#ifndef ASTRALIM_COM_FETCHER_H
#define ASTRALIM_COM_FETCHER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace AstrAlim
{

class AstraComDevice;
class AstraComActor;

class AstraCycleTimer
{
public:
    explicit AstraCycleTimer(double periodS);
    ~AstraCycleTimer();

    void setPeriod(double periodS);
    void start();
    bool waitNextCycle();
    void requestStop();
    double getPeriod() const;
    void close();

private:
    bool setupCycleTimer();

    mutable std::mutex timerLock;
    std::atomic<bool> stopRequested {false};
    double periodS = 0.25;
    int timerFd = -1;
    double nextCycleDeadlineS = 0.0;
    bool hasDeadline = false;
};

class AstraComFetcher
{
public:
    static constexpr double ACTIVATION_TOLERANCE_RATIO = 0.10;
    static constexpr double PHASE_DELAY_S = 0.001;
    static constexpr double CYCLE_PERIOD_S = 0.25;
    static constexpr int CYCLE_STEP_COUNT = 10;

    static AstraComFetcher& getInstance();
    static void exitAll();

    AstraComFetcher(const AstraComFetcher&) = delete;
    AstraComFetcher& operator=(const AstraComFetcher&) = delete;

    void addDevice(AstraComDevice& device, bool eachStep = false);
    void addActor(AstraComActor& actor);
    void removeActor(AstraComActor& actor);

    AstraCycleTimer& getCycleTimer();

    double getCyclePeriod() const;
    void setCyclePeriod(double periodS);

    int getCycleStepCount() const;
    void setCycleStepCount(int stepCount);

    void start();
    void stop();

private:
    AstraComFetcher();
    ~AstraComFetcher();

    void run();

    void notifyCycleConfigurationChanged();
    void reassignStepIndexedDevicesNoLock();
    void checkActivationTiming(double activationTs, int step, double executionDurationS);

    mutable std::mutex devicesLock;
    std::vector<AstraComDevice*> eachStepDevices;
    std::vector<std::vector<AstraComDevice*>> stepIndexedDevices;

    mutable std::mutex actorsLock;
    std::vector<AstraComActor*> actors;

    std::atomic<bool> running {false};
    std::atomic<bool> hasSharedUpdate {false};

    double cyclePeriodS = CYCLE_PERIOD_S;
    int cycleStepCount = CYCLE_STEP_COUNT;
    AstraCycleTimer cycleTimer;
    double lastActivationTs = 0.0;
    bool hasLastActivationTs = false;

    mutable std::mutex stateLock;
    std::thread worker;
};

} // namespace AstrAlim

#endif // ASTRALIM_COM_FETCHER_H