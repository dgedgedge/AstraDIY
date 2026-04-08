#include "astralim_1wire_temp_fetcher.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace AstrAlim
{

Astra1WireTempFetcher& Astra1WireTempFetcher::getInstance()
{
    static Astra1WireTempFetcher instance;
    return instance;
}

Astra1WireTempFetcher::Astra1WireTempFetcher()
{
    worker = std::thread(&Astra1WireTempFetcher::run, this);
}

Astra1WireTempFetcher::~Astra1WireTempFetcher()
{
    stop();
}

void Astra1WireTempFetcher::stop()
{
    bool expected = false;
    if (!stopRequested.compare_exchange_strong(expected, true))
        return;

    if (worker.joinable())
        worker.join();
}

void Astra1WireTempFetcher::run()
{
    while (!stopRequested.load())
    {
        const auto cycleStart = std::chrono::steady_clock::now();

        try
        {
            updateTempList();
            readOneSensor();
        }
        catch (...)
        {
        }

        const auto elapsed = std::chrono::steady_clock::now() - cycleStart;
        const auto target = std::chrono::duration<double>(CYCLE_PERIOD_S);
        if (elapsed < target)
            std::this_thread::sleep_for(target - elapsed);
    }
}

void Astra1WireTempFetcher::updateTempList()
{
    bool hasChange = false;
    const std::filesystem::path basePath("/sys/bus/w1/devices");

    if (std::filesystem::exists(basePath) && std::filesystem::is_directory(basePath))
    {
        for (const auto& entry : std::filesystem::directory_iterator(basePath))
        {
            if (!entry.is_directory())
                continue;

            const std::string name = entry.path().filename().string();
            if (name.rfind("28", 0) != 0)
                continue;

            const std::filesystem::path sensorFile = entry.path() / "w1_slave";
            if (!std::filesystem::exists(sensorFile))
                continue;

            std::lock_guard<std::mutex> guard(lock);
            if (tableTemp.find(name) == tableTemp.end())
            {
                tableTemp[name] = SensorEntry {0.0, sensorFile.string(), 0};
                hasChange = true;
            }
        }
    }

    std::lock_guard<std::mutex> guard(lock);
    if (hasChange || tempNames.empty())
    {
        tempNames.clear();
        tempNames.reserve(tableTemp.size());
        for (const auto& item : tableTemp)
            tempNames.push_back(item.first);

        if (tempNames.empty())
            nextSensorIndex = 0;
        else if (nextSensorIndex >= tempNames.size())
            nextSensorIndex = 0;
    }
}

std::vector<std::string> Astra1WireTempFetcher::readTempLines(const std::string& path) const
{
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
    {
        lines.push_back(line);
    }
    return lines;
}

void Astra1WireTempFetcher::readOneSensor()
{
    std::string tempName;
    std::string tempFile;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (tempNames.empty())
            return;

        if (nextSensorIndex >= tempNames.size())
            nextSensorIndex = 0;

        tempName = tempNames[nextSensorIndex];
        tempFile = tableTemp[tempName].file;
        nextSensorIndex = (nextSensorIndex + 1) % tempNames.size();
    }

    constexpr double invalid = 998.0;
    double returnValue = invalid;
    try
    {
        const auto lines = readTempLines(tempFile);
        if (lines.size() == 2 && lines[0].size() >= 3 && lines[0].substr(lines[0].size() - 3) == "YES")
        {
            const auto pos = lines[1].find("t=");
            if (pos != std::string::npos)
            {
                const auto raw = lines[1].substr(pos + 2);
                returnValue = std::stod(raw) / 1000.0;
            }
        }
    }
    catch (...)
    {
    }

    std::lock_guard<std::mutex> guard(lock);
    auto it = tableTemp.find(tempName);
    if (it == tableTemp.end())
        return;

    if (returnValue != invalid)
    {
        it->second.value = returnValue;
        it->second.failCount = 0;
    }
    else
    {
        it->second.value = TEMPUNAVAIL;
        it->second.failCount += 1;
    }
}

std::vector<std::string> Astra1WireTempFetcher::getListTemp() const
{
    std::lock_guard<std::mutex> guard(lock);
    std::vector<std::string> keys;
    keys.reserve(tableTemp.size());
    for (const auto& item : tableTemp)
        keys.push_back(item.first);
    return keys;
}

double Astra1WireTempFetcher::getTemp(const std::string& tempName) const
{
    std::lock_guard<std::mutex> guard(lock);
    const auto it = tableTemp.find(tempName);
    if (it == tableTemp.end())
        return TEMPUNAVAIL;
    return it->second.value;
}

int Astra1WireTempFetcher::getFailCount(const std::string& tempName) const
{
    std::lock_guard<std::mutex> guard(lock);
    const auto it = tableTemp.find(tempName);
    if (it == tableTemp.end())
        return 0;
    return it->second.failCount;
}

std::map<std::string, Astra1WireTempFetcher::SensorEntry> Astra1WireTempFetcher::getSnapshot() const
{
    std::lock_guard<std::mutex> guard(lock);
    return tableTemp;
}

} // namespace AstrAlim