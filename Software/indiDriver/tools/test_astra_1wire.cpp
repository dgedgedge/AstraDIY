#include "astralim_1wire_temp_fetcher.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    int runMs = 2500;
    if (argc > 1)
    {
        runMs = std::atoi(argv[1]);
        if (runMs < 1000)
            runMs = 1000;
    }

    auto& fetcher = AstrAlim::Astra1WireTempFetcher::getInstance();
    std::this_thread::sleep_for(std::chrono::milliseconds(runMs));

    const auto snapshot = fetcher.getSnapshot();
    std::cout << "runMs=" << runMs << '\n';
    std::cout << "sensorCount=" << snapshot.size() << '\n';

    for (const auto& item : snapshot)
    {
        std::cout << item.first
                  << " tempC=" << item.second.value
                  << " failCount=" << item.second.failCount
                  << '\n';
    }

    fetcher.stop();
    std::cout << "status=OK" << '\n';
    return 0;
}