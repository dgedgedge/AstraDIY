#include "astralim_com_fetcher.h"

#include "astralim_com_actor.h"
#include "astralim_com_device.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

namespace AstrAlim
{

namespace
{
double monotonicNowSeconds()
{
    using Clock = std::chrono::steady_clock;
    const auto now = Clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}
} // namespace

AstraCycleTimer::AstraCycleTimer(double periodValue)
    : periodS(periodValue)
{
    if (periodS <= 0.0)
        throw std::invalid_argument("periodS must be greater than 0");
}

AstraCycleTimer::~AstraCycleTimer()
{
    requestStop();
}

bool AstraCycleTimer::setupCycleTimer()
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (fd < 0)
        return false;

    itimerspec spec {};
    {
        std::lock_guard<std::mutex> lock(timerLock);
        const auto sec = static_cast<time_t>(periodS);
        const auto nsec = static_cast<long>((periodS - static_cast<double>(sec)) * 1'000'000'000.0);
        spec.it_interval.tv_sec = sec;
        spec.it_interval.tv_nsec = nsec;
        spec.it_value.tv_sec = sec;
        spec.it_value.tv_nsec = nsec;
    }

    if (timerfd_settime(fd, 0, &spec, nullptr) != 0)
    {
        ::close(fd);
        return false;
    }

    std::lock_guard<std::mutex> lock(timerLock);
    timerFd = fd;
    return true;
}

void AstraCycleTimer::setPeriod(double periodValue)
{
    if (periodValue <= 0.0)
        throw std::invalid_argument("periodS must be greater than 0");

    {
        std::lock_guard<std::mutex> lock(timerLock);
        periodS = periodValue;
        hasDeadline = false;
    }

    const bool hadTimerFd = [&]() {
        std::lock_guard<std::mutex> lock(timerLock);
        return timerFd >= 0;
    }();

    if (hadTimerFd)
    {
        close();
        setupCycleTimer();
    }
}

void AstraCycleTimer::start()
{
    stopRequested.store(false);
    {
        std::lock_guard<std::mutex> lock(timerLock);
        hasDeadline = false;
    }
    setupCycleTimer();
}

bool AstraCycleTimer::waitNextCycle()
{
    if (stopRequested.load())
        return false;

    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(timerLock);
        fd = timerFd;
    }

    if (fd < 0)
    {
        const double now = monotonicNowSeconds();
        double deadline = 0.0;
        double period = 0.0;
        {
            std::lock_guard<std::mutex> lock(timerLock);
            if (!hasDeadline)
            {
                nextCycleDeadlineS = now + periodS;
                hasDeadline = true;
            }
            deadline = nextCycleDeadlineS;
            period = periodS;
        }

        const double remaining = deadline - now;
        if (remaining > 0.0)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(std::max(remaining, 0.001)));
            if (stopRequested.load())
                return false;
        }

        const double nowAfterSleep = monotonicNowSeconds();
        {
            std::lock_guard<std::mutex> lock(timerLock);
            while (hasDeadline && nextCycleDeadlineS <= nowAfterSleep)
            {
                nextCycleDeadlineS += period;
            }
        }
        return !stopRequested.load();
    }

    uint64_t expirations = 0;
    const auto bytesRead = ::read(fd, &expirations, sizeof(expirations));
    if (bytesRead == static_cast<ssize_t>(sizeof(expirations)))
        return !stopRequested.load();

    if (stopRequested.load())
        return false;

    close();
    return waitNextCycle();
}

void AstraCycleTimer::requestStop()
{
    stopRequested.store(true);
    close();
}

double AstraCycleTimer::getPeriod() const
{
    std::lock_guard<std::mutex> lock(timerLock);
    return periodS;
}

void AstraCycleTimer::close()
{
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(timerLock);
        fd = timerFd;
        timerFd = -1;
    }

    if (fd >= 0)
    {
        ::close(fd);
    }
}

AstraComFetcher::AstraComFetcher()
    : cycleTimer(CYCLE_PERIOD_S)
{
    stepIndexedDevices.resize(cycleStepCount);
    start();
}

AstraComFetcher::~AstraComFetcher()
{
    stop();
}

AstraComFetcher& AstraComFetcher::getInstance()
{
    static AstraComFetcher instance;
    return instance;
}

void AstraComFetcher::exitAll()
{
    getInstance().stop();
}

void AstraComFetcher::start()
{
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true))
        return;

    worker = std::thread(&AstraComFetcher::run, this);
}

void AstraComFetcher::stop()
{
    bool expected = true;
    if (!running.compare_exchange_strong(expected, false))
        return;

    cycleTimer.requestStop();
    if (worker.joinable())
        worker.join();
}

void AstraComFetcher::addDevice(AstraComDevice& device, bool eachStep)
{
    hasSharedUpdate.store(true);
    {
        std::lock_guard<std::mutex> lock(devicesLock);
        if (eachStep)
        {
            eachStepDevices.push_back(&device);
        }
        else
        {
            if (stepIndexedDevices.empty())
                stepIndexedDevices.resize(cycleStepCount);
            stepIndexedDevices[0].push_back(&device);
        }
    }

    notifyCycleConfigurationChanged();
}

void AstraComFetcher::addActor(AstraComActor& actor)
{
    hasSharedUpdate.store(true);
    {
        std::lock_guard<std::mutex> lock(actorsLock);
        actors.push_back(&actor);
    }

    notifyCycleConfigurationChanged();
}

void AstraComFetcher::removeActor(AstraComActor& actor)
{
    hasSharedUpdate.store(true);
    std::lock_guard<std::mutex> lock(actorsLock);
    actors.erase(std::remove(actors.begin(), actors.end(), &actor), actors.end());
}

AstraCycleTimer& AstraComFetcher::getCycleTimer()
{
    return cycleTimer;
}

double AstraComFetcher::getCyclePeriod() const
{
    std::lock_guard<std::mutex> lock(stateLock);
    return cyclePeriodS;
}

void AstraComFetcher::setCyclePeriod(double periodValue)
{
    if (periodValue <= 0.0)
        throw std::invalid_argument("periodS must be greater than 0");

    hasSharedUpdate.store(true);
    {
        std::lock_guard<std::mutex> lock(stateLock);
        cyclePeriodS = periodValue;
    }

    cycleTimer.setPeriod(periodValue);
    notifyCycleConfigurationChanged();
}

int AstraComFetcher::getCycleStepCount() const
{
    std::lock_guard<std::mutex> lock(stateLock);
    return cycleStepCount;
}

void AstraComFetcher::setCycleStepCount(int stepCount)
{
    if (stepCount <= 0)
        throw std::invalid_argument("stepCount must be greater than 0");

    hasSharedUpdate.store(true);
    {
        std::lock_guard<std::mutex> lock(stateLock);
        cycleStepCount = stepCount;
    }

    notifyCycleConfigurationChanged();
}

void AstraComFetcher::notifyCycleConfigurationChanged()
{
    double periodValue = 0.0;
    int stepCountValue = 0;
    {
        std::lock_guard<std::mutex> lock(stateLock);
        periodValue = cyclePeriodS;
        stepCountValue = cycleStepCount;
    }

    std::vector<AstraComActor*> actorsSnapshot;
    {
        std::lock_guard<std::mutex> lock(actorsLock);
        actorsSnapshot = actors;
    }

    std::vector<AstraComDevice*> devicesSnapshot;
    {
        std::lock_guard<std::mutex> lock(devicesLock);
        reassignStepIndexedDevicesNoLock();
        devicesSnapshot = eachStepDevices;
        for (const auto& bucket : stepIndexedDevices)
        {
            devicesSnapshot.insert(devicesSnapshot.end(), bucket.begin(), bucket.end());
        }
    }

    for (auto* actor : actorsSnapshot)
    {
        try
        {
            if (actor)
                actor->onCycleConfigurationChanged(periodValue, stepCountValue);
        }
        catch (...)
        {
        }
    }

    for (auto* device : devicesSnapshot)
    {
        try
        {
            if (device)
                device->onCycleConfigurationChanged(periodValue, stepCountValue);
        }
        catch (...)
        {
        }
    }

    std::cout << "[DEBUG AstraComFetcher] Cycle configuration changed: period="
              << periodValue
              << "s steps="
              << stepCountValue
              << " devices="
              << devicesSnapshot.size()
              << " actors="
              << actorsSnapshot.size()
              << std::endl;
}

void AstraComFetcher::reassignStepIndexedDevicesNoLock()
{
    std::vector<AstraComDevice*> allStepDevices;
    for (const auto& bucket : stepIndexedDevices)
    {
        allStepDevices.insert(allStepDevices.end(), bucket.begin(), bucket.end());
    }

    int stepCountValue = 0;
    {
        std::lock_guard<std::mutex> lock(stateLock);
        stepCountValue = cycleStepCount;
    }

    stepIndexedDevices.assign(stepCountValue, {});
    for (size_t index = 0; index < allStepDevices.size(); ++index)
    {
        const int targetStep = static_cast<int>(index % static_cast<size_t>(stepCountValue));
        stepIndexedDevices[targetStep].push_back(allStepDevices[index]);
    }
}

void AstraComFetcher::checkActivationTiming(double activationTs, int step, double executionDurationS)
{
    double expectedPeriodS = 0.0;
    {
        std::lock_guard<std::mutex> lock(stateLock);
        expectedPeriodS = cyclePeriodS;
    }

    if (expectedPeriodS <= 0.0)
    {
        lastActivationTs = activationTs;
        hasLastActivationTs = true;
        return;
    }

    const double previousActivationTs = lastActivationTs;
    const bool hadPrevious = hasLastActivationTs;
    lastActivationTs = activationTs;
    hasLastActivationTs = true;
    if (!hadPrevious)
        return;

    const double actualPeriodS = activationTs - previousActivationTs;
    const double allowedDeltaS = expectedPeriodS * ACTIVATION_TOLERANCE_RATIO;
    if (std::fabs(actualPeriodS - expectedPeriodS) > allowedDeltaS)
    {
        const double expectedMs = expectedPeriodS * 1000.0;
        const double actualMs = actualPeriodS * 1000.0;
        const double allowedMs = allowedDeltaS * 1000.0;
        const double errorMs = std::fabs(actualPeriodS - expectedPeriodS) * 1000.0;

        std::cerr << "[ERROR AstraComFetcher] Activation timing out of tolerance "
                  << "(did you set I2c to 40KHz in firmware): "
                  << "step=" << step
                  << " expected=" << expectedMs << "ms"
                  << " actual=" << actualMs << "ms"
                  << " runExec=" << (executionDurationS * 1000.0) << "ms"
                  << " error=" << errorMs << "ms"
                  << " tolerance=" << allowedMs << "ms"
                  << std::endl;
    }
}

void AstraComFetcher::run()
{
    std::vector<AstraComActor*> actorsSnapshot;
    std::vector<AstraComDevice*> eachStepSnapshot;
    std::vector<std::vector<AstraComDevice*>> stepIndexedSnapshot;

    {
        std::lock_guard<std::mutex> lock(actorsLock);
        actorsSnapshot = actors;
    }
    {
        std::lock_guard<std::mutex> lock(devicesLock);
        eachStepSnapshot = eachStepDevices;
        stepIndexedSnapshot = stepIndexedDevices;
    }

    int cycleStep = 0;
    double executionDurationS = 0.0;

    cycleTimer.start();

    while (running.load())
    {
        if (!cycleTimer.waitNextCycle())
            break;

        if (hasSharedUpdate.exchange(false))
        {
            std::lock_guard<std::mutex> lockActors(actorsLock);
            actorsSnapshot = actors;
            std::lock_guard<std::mutex> lockDevices(devicesLock);
            eachStepSnapshot = eachStepDevices;
            stepIndexedSnapshot = stepIndexedDevices;
        }

        try
        {
            const double executionStartTs = monotonicNowSeconds();

            double periodValue = 0.0;
            int stepCountValue = 0;
            {
                std::lock_guard<std::mutex> lock(stateLock);
                periodValue = cyclePeriodS;
                stepCountValue = cycleStepCount;
            }

            const double stepIntegrationDurationS = periodValue;
            const double cycleIntegrationDurationS = periodValue * static_cast<double>(stepCountValue);

            checkActivationTiming(executionStartTs, cycleStep, executionDurationS);

            for (auto* actor : actorsSnapshot)
            {
                try
                {
                    if (actor)
                        actor->beforeMeasurements(cycleStep);
                }
                catch (...)
                {
                }
            }

            std::this_thread::sleep_for(std::chrono::duration<double>(PHASE_DELAY_S));

            std::vector<AstraComDevice*> stepDevices;
            if (cycleStep >= 0 && cycleStep < static_cast<int>(stepIndexedSnapshot.size()))
            {
                stepDevices = stepIndexedSnapshot[cycleStep];
            }

            for (auto* device : eachStepSnapshot)
            {
                try
                {
                    if (device)
                        device->startMeasurement(cycleStep, stepIntegrationDurationS);
                }
                catch (...)
                {
                }
            }
            for (auto* device : stepDevices)
            {
                try
                {
                    if (device)
                        device->startMeasurement(cycleStep, cycleIntegrationDurationS);
                }
                catch (...)
                {
                }
            }

            std::this_thread::sleep_for(std::chrono::duration<double>(PHASE_DELAY_S));

            for (auto* device : eachStepSnapshot)
            {
                try
                {
                    if (device)
                        device->getMeasurement(cycleStep, stepIntegrationDurationS);
                }
                catch (...)
                {
                }
            }
            for (auto* device : stepDevices)
            {
                try
                {
                    if (device)
                        device->getMeasurement(cycleStep, cycleIntegrationDurationS);
                }
                catch (...)
                {
                }
            }

            executionDurationS = monotonicNowSeconds() - executionStartTs;
            if (stepCountValue > 0)
                cycleStep = (cycleStep + 1) % stepCountValue;
            else
                cycleStep = 0;
        }
        catch (...)
        {
        }
    }

    cycleTimer.close();
}

} // namespace AstrAlim