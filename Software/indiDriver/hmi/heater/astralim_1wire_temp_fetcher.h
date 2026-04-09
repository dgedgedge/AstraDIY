#ifndef ASTRALIM_1WIRE_TEMP_FETCHER_H
#define ASTRALIM_1WIRE_TEMP_FETCHER_H

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace AstrAlim
{

class Astra1WireTempFetcher
{
public:
    struct SensorEntry
    {
        double value = 0.0;
        std::string file;
        int failCount = 0;
    };

    static constexpr double TEMPUNAVAIL = 100.0;
    static constexpr double CYCLE_PERIOD_S = 1.0;

    static Astra1WireTempFetcher& getInstance();

    Astra1WireTempFetcher(const Astra1WireTempFetcher&) = delete;
    Astra1WireTempFetcher& operator=(const Astra1WireTempFetcher&) = delete;

    void stop();

    std::vector<std::string> getListTemp() const;
    double getTemp(const std::string& tempName) const;
    int getFailCount(const std::string& tempName) const;
    std::map<std::string, SensorEntry> getSnapshot() const;

private:
    Astra1WireTempFetcher();
    ~Astra1WireTempFetcher();

    void run();
    void updateTempList();
    void readOneSensor();
    std::vector<std::string> readTempLines(const std::string& path) const;

    mutable std::mutex lock;
    std::map<std::string, SensorEntry> tableTemp;

    size_t nextSensorIndex = 0;
    std::vector<std::string> tempNames;

    std::atomic<bool> stopRequested {false};
    std::thread worker;
};

} // namespace AstrAlim

#endif // ASTRALIM_1WIRE_TEMP_FETCHER_H