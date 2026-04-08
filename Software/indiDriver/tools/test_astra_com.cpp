#include "astralim_com_actor.h"
#include "astralim_com_device.h"
#include "astralim_com_fetcher.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{

class DemoActor : public AstrAlim::AstraComActor
{
public:
    DemoActor()
        : AstrAlim::AstraComActor("DemoActor")
    {
    }

    void beforeMeasurements(int) override
    {
        beforeCount.fetch_add(1, std::memory_order_relaxed);
    }

    std::atomic<int> beforeCount {0};
};

class DemoEachStepDevice : public AstrAlim::AstraComDevice
{
public:
    DemoEachStepDevice()
        : AstrAlim::AstraComDevice(true, "DemoEachStepDevice")
    {
    }

    void startMeasurement(int, double) override
    {
        startCount.fetch_add(1, std::memory_order_relaxed);
    }

    void getMeasurement(int, double) override
    {
        measureCount.fetch_add(1, std::memory_order_relaxed);
    }

    std::atomic<int> startCount {0};
    std::atomic<int> measureCount {0};
};

class DemoStepDevice : public AstrAlim::AstraComDevice
{
public:
    DemoStepDevice()
        : AstrAlim::AstraComDevice(false, "DemoStepDevice")
    {
    }

    void startMeasurement(int, double) override
    {
        startCount.fetch_add(1, std::memory_order_relaxed);
    }

    void getMeasurement(int, double) override
    {
        measureCount.fetch_add(1, std::memory_order_relaxed);
    }

    std::atomic<int> startCount {0};
    std::atomic<int> measureCount {0};
};

} // namespace

int main(int argc, char** argv)
{
    int runMs = 1200;
    if (argc > 1)
    {
        runMs = std::atoi(argv[1]);
        if (runMs < 250)
            runMs = 250;
    }

    AstrAlim::AstraComFetcher& fetcher = AstrAlim::AstraComFetcher::getInstance();
    fetcher.setCyclePeriod(0.1);
    fetcher.setCycleStepCount(5);

    DemoActor actor;
    DemoEachStepDevice eachStepDevice;
    DemoStepDevice stepDevice;

    std::this_thread::sleep_for(std::chrono::milliseconds(runMs));

    AstrAlim::AstraComFetcher::exitAll();

    std::cout << "runMs=" << runMs << '\n';
    std::cout << "actor.beforeMeasurements=" << actor.beforeCount.load() << '\n';
    std::cout << "eachStep.start=" << eachStepDevice.startCount.load() << '\n';
    std::cout << "eachStep.get=" << eachStepDevice.measureCount.load() << '\n';
    std::cout << "stepIndexed.start=" << stepDevice.startCount.load() << '\n';
    std::cout << "stepIndexed.get=" << stepDevice.measureCount.load() << '\n';

    const bool ok = actor.beforeCount.load() > 0 &&
                    eachStepDevice.measureCount.load() > 0 &&
                    stepDevice.measureCount.load() > 0;

    std::cout << "status=" << (ok ? "OK" : "FAIL") << '\n';
    return ok ? 0 : 1;
}