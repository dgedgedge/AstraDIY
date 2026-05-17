#include "lib/internal/astralim_bme280.h"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[])
{
    int address = AstrAlim::Bme280::DEFAULT_DEVICE;
    int busId = 1;
    int sampleCount = 1;

    if (argc > 1)
        address = std::stoi(argv[1], nullptr, 0); // Accept 0x76 format.
    if (argc > 2)
        busId = std::stoi(argv[2]);
    if (argc > 3)
        sampleCount = std::max(1, std::stoi(argv[3]));

    try
    {
        AstrAlim::Bme280 sensor(address, busId);

        const auto [chipId, chipVersion] = sensor.readBME280ID();
        std::cout << "BME280 detected on /dev/i2c-" << busId
                  << " addr=0x" << std::hex << address << std::dec
                  << " chipId=0x" << std::hex << chipId << std::dec
                  << " version=" << chipVersion << '\n';

        sensor.loadCalibrationData();
        sensor.configureNormalMode();

        for (int i = 0; i < sampleCount; ++i)
        {
            const auto [temperature, pressure, humidity] = sensor.readBME280All();
            std::cout << std::fixed << std::setprecision(2)
                      << "sample=" << (i + 1)
                      << " tempC=" << temperature
                      << " pressure_hPa=" << pressure
                      << " humidity_pct=" << humidity
                      << '\n';

            if (i + 1 < sampleCount)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "BME280 test failed: " << ex.what() << '\n';
        return 1;
    }
}
