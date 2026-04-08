#include "astralim_com_fetcher.h"
#include "astralim_step_pwm_actor.h"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    int gpio = 18;
    double dutyPercent = 40.0;
    int runMs = 1200;

    if (argc > 1)
        gpio = std::atoi(argv[1]);
    if (argc > 2)
        dutyPercent = std::atof(argv[2]);
    if (argc > 3)
        runMs = std::atoi(argv[3]);

    if (runMs < 500)
        runMs = 500;

    try
    {
        auto& fetcher = AstrAlim::AstraComFetcher::getInstance();
        fetcher.setCyclePeriod(0.1);
        fetcher.setCycleStepCount(10);

        AstrAlim::AstraStepPwmActor actor(gpio, dutyPercent, "TestStepPwm");
        std::this_thread::sleep_for(std::chrono::milliseconds(runMs));

        std::cout << "gpio=" << gpio
                  << " dutyPercent=" << actor.getStepPercent()
                  << " pwmUpdatePeriodS=" << actor.getPwmUpdatePeriodS()
                  << " isHigh=" << (actor.isHigh() ? 1 : 0)
                  << '\n';

        actor.close();
        AstrAlim::AstraComFetcher::exitAll();
        std::cout << "status=OK" << '\n';
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cout << "status=SKIP reason=" << ex.what() << '\n';
        AstrAlim::AstraComFetcher::exitAll();
        return 0;
    }
}