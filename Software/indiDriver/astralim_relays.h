/*******************************************************************************
 * AstrAlim Relays Driver - Modern INDI API
 * Copyright (c) 2024 AstrAlim Project
 * Based on original work by Radek Kaczorek
 ******************************************************************************/

#ifndef ASTRALIM_RELAYS_H
#define ASTRALIM_RELAYS_H

#include <defaultdevice.h>
#include <memory>
#include <array>
#include "astralim_gpio.h"

namespace AstrAlim {
class AstraIna;
}

class AstrAlimRelays : public INDI::DefaultDevice
{
public:
    AstrAlimRelays();
    virtual ~AstrAlimRelays() = default;

    const char* getDefaultName() override;
    
    bool initProperties() override;
    bool updateProperties() override;
    
    bool ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n) override;
    bool ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n) override;

protected:
    bool Connect() override;
    bool Disconnect() override;
    void TimerHit() override;
    bool saveConfigItems(FILE* fp) override;

private:
    void updateSwitchStates();
    bool setRelay(int relay, bool on);
    void readINA219();
    // GPIO controller
    std::unique_ptr<AstrAlim::GpioController> gpio;
    std::array<std::unique_ptr<AstrAlim::AstraIna>, 3> inaSensors;

    // Properties - Active state
    INDI::PropertySwitch ActiveStateSP {2};
    enum { STATE_LOW, STATE_HIGH };
    
    // Properties - Relay switches
    INDI::PropertySwitch Relay1SP {2};
    INDI::PropertySwitch Relay2SP {2};
    INDI::PropertySwitch Relay3SP {2};
    enum { RELAY_ON, RELAY_OFF };
    
    // Properties - BCM Pins (configurable)
    INDI::PropertyNumber BCMPinsNP {3};
    
    // Properties - Power monitoring (INA219)
    INDI::PropertyNumber PowerDC1NP {3};  // Voltage, Current, Power
    INDI::PropertyNumber PowerDC2NP {3};
    INDI::PropertyNumber PowerDC3NP {3};
    INDI::PropertyNumber TotalPowerNP {2};  // Total current, Total energy
    enum { PWR_VOLTAGE, PWR_CURRENT, PWR_POWER };

    // State
    int activeState = 0;  // 0 = active low, 1 = active high
    int relayState[3] = {0, 0, 0};
    double totalEnergymWh = 0;
    
    static constexpr int POLLING_MS = 1000;
    
    // INA219 I2C addresses
    static constexpr int INA_DC1_ADDR = 0x41;
    static constexpr int INA_DC2_ADDR = 0x44;
    static constexpr int INA_DC3_ADDR = 0x46;
};

#endif // ASTRALIM_RELAYS_H

