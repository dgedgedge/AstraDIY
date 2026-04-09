#ifndef ASTRALIM_BME280_H
#define ASTRALIM_BME280_H

#include <cstdint>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

namespace AstrAlim
{

class Bme280
{
public:
    static constexpr int DEFAULT_DEVICE = 0x76;

    explicit Bme280(int addr = DEFAULT_DEVICE, int busId = 1);
    ~Bme280();

    int getShort(const std::vector<uint8_t>& data, size_t index) const;
    int getUShort(const std::vector<uint8_t>& data, size_t index) const;
    int getChar(const std::vector<uint8_t>& data, size_t index) const;
    int getUChar(const std::vector<uint8_t>& data, size_t index) const;

    std::pair<int, int> readBME280ID(int addr = -1);
    void loadCalibrationData(int addr = -1);

    void configureNormalMode(int addr = -1,
                             int oversampleTemp = 2,
                             int oversamplePres = 2,
                             int oversampleHum = 2,
                             int standbyCode = 5,
                             int filterCode = 0);

    std::tuple<double, double, double> acquireDataNormalMode(int addr = -1);
    std::tuple<double, double, double> readBME280All(int addr = -1);

private:
    int addr;
    int busId;
    int fd = -1;

    bool calibrationLoaded = false;
    bool normalModeConfigured = false;
    double normalModeWaitTimeS = 0.0;

    int digT1 = 0;
    int digT2 = 0;
    int digT3 = 0;

    int digP1 = 0;
    int digP2 = 0;
    int digP3 = 0;
    int digP4 = 0;
    int digP5 = 0;
    int digP6 = 0;
    int digP7 = 0;
    int digP8 = 0;
    int digP9 = 0;

    int digH1 = 0;
    int digH2 = 0;
    int digH3 = 0;
    int digH4 = 0;
    int digH5 = 0;
    int digH6 = 0;

    static std::mutex busMapMutex;
    static std::map<int, std::shared_ptr<std::mutex>> busLocks;
    std::shared_ptr<std::mutex> busLock;

    void openBus();
    void closeBus();
    int effectiveAddr(int requestedAddr) const;

    uint8_t readByteData(int deviceAddr, uint8_t reg);
    void writeByteData(int deviceAddr, uint8_t reg, uint8_t value);
    std::vector<uint8_t> readI2cBlockData(int deviceAddr, uint8_t reg, size_t len);
};

int getShort(const std::vector<uint8_t>& data, size_t index);
int getUShort(const std::vector<uint8_t>& data, size_t index);
int getChar(const std::vector<uint8_t>& data, size_t index);
int getUChar(const std::vector<uint8_t>& data, size_t index);

std::pair<int, int> readBME280ID(int addr = Bme280::DEFAULT_DEVICE);
std::tuple<double, double, double> readBME280All(int addr = Bme280::DEFAULT_DEVICE);

} // namespace AstrAlim

#endif // ASTRALIM_BME280_H
