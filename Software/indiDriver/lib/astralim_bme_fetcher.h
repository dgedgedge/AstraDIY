#ifndef ASTRALIM_BME_FETCHER_H
#define ASTRALIM_BME_FETCHER_H

#include "internal/astralim_bme280.h"
#include "astralim_com_device.h"

#include <memory>
#include <vector>

namespace AstrAlim
{

class AstraBmeFetcher : public AstraComDevice
{
public:
    static constexpr double ROSEEUNAVAIL = -100.0;
    static constexpr double TEMPUNAVAIL = 100.0;

    static AstraBmeFetcher& getInstance();

    AstraBmeFetcher();

    void getMeasurement(int step, double integrationDurationS) override;

    double getBmeTemp() const;
    double getBmePressure() const;
    double getBmeHumidity() const;
    double getBmeTempRosee() const;

    double getFilteredTemp() const;
    double getFilteredHumidity() const;
    double getFilteredDewPoint() const;

    bool isPresentBme() const;
    bool hasHumiditySensor() const;
    void setManualHumidity(double humidity);

private:
    double filterValue(double newValue, std::vector<double>& history, double minChange);
    double filterDewPoint(double newDewPoint);
    void clearFilters();

    std::unique_ptr<Bme280> bmeSensor;
    bool bmePresent = false;
    double bmeTemperature = TEMPUNAVAIL;
    double bmePressure = 0.0;
    double bmeHumidity = 0.0;
    double bmeTempRosee = ROSEEUNAVAIL;
    bool hasManualHumidity = false;
    double manualHumidity = 0.0;

    int dewPointFilterSize = 10;
    double dewPointMinChange = 0.1;
    double tempHumidityMinChange = 0.05;

    std::vector<double> dewPointHistory;
    std::vector<double> tempHistory;
    std::vector<double> humidityHistory;

    double filteredDewPoint = ROSEEUNAVAIL;
    double filteredTemp = 0.0;
    double filteredHumidity = 0.0;
};

} // namespace AstrAlim

#endif // ASTRALIM_BME_FETCHER_H