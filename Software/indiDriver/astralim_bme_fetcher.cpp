#include "astralim_bme_fetcher.h"

#include <cmath>
#include <stdexcept>

namespace AstrAlim
{

AstraBmeFetcher& AstraBmeFetcher::getInstance()
{
    static AstraBmeFetcher instance;
    return instance;
}

AstraBmeFetcher::AstraBmeFetcher()
    : AstraComDevice(false, "BME280")
{
    try
    {
        bmeSensor = std::make_unique<Bme280>();
        bmeSensor->loadCalibrationData();
        bmeSensor->configureNormalMode();
        bmePresent = true;
    }
    catch (...)
    {
        bmeSensor.reset();
        bmePresent = false;
    }
}

double AstraBmeFetcher::filterValue(double newValue, std::vector<double>& history, double minChange)
{
    if (history.empty())
    {
        history.push_back(newValue);
        return newValue;
    }

    double sum = 0.0;
    for (double value : history)
        sum += value;
    const double average = sum / static_cast<double>(history.size());
    const double change = std::fabs(newValue - average);

    if (change < minChange && history.size() >= 3)
        return average;

    history.push_back(newValue);
    while (static_cast<int>(history.size()) > dewPointFilterSize)
        history.erase(history.begin());

    sum = 0.0;
    for (double value : history)
        sum += value;
    return sum / static_cast<double>(history.size());
}

double AstraBmeFetcher::filterDewPoint(double newDewPoint)
{
    if (dewPointHistory.empty())
    {
        dewPointHistory.push_back(newDewPoint);
        filteredDewPoint = newDewPoint;
        return newDewPoint;
    }

    double sum = 0.0;
    for (double value : dewPointHistory)
        sum += value;
    const double average = sum / static_cast<double>(dewPointHistory.size());
    const double change = std::fabs(newDewPoint - average);

    if (change < dewPointMinChange && dewPointHistory.size() >= 3)
    {
        filteredDewPoint = average;
        return average;
    }

    dewPointHistory.push_back(newDewPoint);
    while (static_cast<int>(dewPointHistory.size()) > dewPointFilterSize)
        dewPointHistory.erase(dewPointHistory.begin());

    sum = 0.0;
    for (double value : dewPointHistory)
        sum += value;
    filteredDewPoint = sum / static_cast<double>(dewPointHistory.size());
    return filteredDewPoint;
}

void AstraBmeFetcher::clearFilters()
{
    dewPointHistory.clear();
    tempHistory.clear();
    humidityHistory.clear();
    filteredDewPoint = ROSEEUNAVAIL;
}

void AstraBmeFetcher::getMeasurement(int, double)
{
    try
    {
        if (!bmeSensor)
            throw std::runtime_error("BME280 not initialized");

        std::tie(bmeTemperature, bmePressure, bmeHumidity) = bmeSensor->acquireDataNormalMode();
        bmePresent = true;

        filteredTemp = filterValue(bmeTemperature, tempHistory, tempHumidityMinChange);

        double humidityToUse = bmeHumidity;
        if (bmeHumidity == 0.0 && hasManualHumidity)
            humidityToUse = manualHumidity;

        filteredHumidity = filterValue(humidityToUse, humidityHistory, tempHumidityMinChange);

        if (humidityToUse > 0.0)
        {
            constexpr double a = 17.27;
            constexpr double b = 237.7;
            const double factor = ((a * filteredTemp) / (b + filteredTemp)) + std::log(filteredHumidity / 100.0);
            const double dewPoint = b * factor / (a - factor);
            bmeTempRosee = filterDewPoint(dewPoint);
        }
        else
        {
            clearFilters();
            bmeTempRosee = ROSEEUNAVAIL;
        }
    }
    catch (...)
    {
        bmePresent = false;
        clearFilters();
        bmeTempRosee = ROSEEUNAVAIL;
        bmeTemperature = TEMPUNAVAIL;
    }
}

double AstraBmeFetcher::getBmeTemp() const { return bmeTemperature; }
double AstraBmeFetcher::getBmePressure() const { return bmePressure; }

double AstraBmeFetcher::getBmeHumidity() const
{
    if (bmeHumidity == 0.0 && hasManualHumidity)
        return manualHumidity;
    return bmeHumidity;
}

double AstraBmeFetcher::getBmeTempRosee() const { return bmeTempRosee; }
double AstraBmeFetcher::getFilteredTemp() const { return filteredTemp; }
double AstraBmeFetcher::getFilteredHumidity() const { return filteredHumidity; }
double AstraBmeFetcher::getFilteredDewPoint() const { return filteredDewPoint; }
bool AstraBmeFetcher::isPresentBme() const { return bmePresent; }
bool AstraBmeFetcher::hasHumiditySensor() const { return bmeHumidity > 0.0; }

void AstraBmeFetcher::setManualHumidity(double humidity)
{
    manualHumidity = humidity;
    hasManualHumidity = true;
}

} // namespace AstrAlim