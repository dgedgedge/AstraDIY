/*******************************************************************************
 * AstrAlim Relays Driver - Modern INDI API Implementation
 * Copyright (c) 2024 AstrAlim Project
 * Based on original work by Radek Kaczorek
 ******************************************************************************/

#include "astralim_relays.h"
#include "config.h"
#include <cstring>
#include <array>
#include <memory>
#include <sstream>

// Singleton instance
static std::unique_ptr<AstrAlimRelays> relaysInstance(new AstrAlimRelays());

AstrAlimRelays::AstrAlimRelays()
{
    setVersion(INDI_ASTRALIM_VERSION_MAJOR, INDI_ASTRALIM_VERSION_MINOR);
    gpio = std::make_unique<AstrAlim::GpioController>();
    
    // Initialize relay states to OFF (inverse of active state)
    for (int i = 0; i < 3; i++)
    {
        relayState[i] = !activeState;
    }
}

const char* AstrAlimRelays::getDefaultName()
{
    return "AstrAlim Relays";
}

bool AstrAlimRelays::initProperties()
{
    INDI::DefaultDevice::initProperties();
    
    // BCM Pins configuration
    BCMPinsNP[0].fill("BCMPIN_DC1", "DC1", "%0.0f", 1, 27, 0, AstrAlim::RelayPins::DC1);
    BCMPinsNP[1].fill("BCMPIN_DC2", "DC2", "%0.0f", 1, 27, 0, AstrAlim::RelayPins::DC2);
    BCMPinsNP[2].fill("BCMPIN_DC3", "DC3", "%0.0f", 1, 27, 0, AstrAlim::RelayPins::DC3);
    BCMPinsNP.fill(getDeviceName(), "BCMPINS", "BCM Pins", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // Active state selector (default: Active High for DIY boards)
    ActiveStateSP[STATE_LOW].fill("ACTIVE_LOW", "Active Low", ISS_OFF);
    ActiveStateSP[STATE_HIGH].fill("ACTIVE_HIGH", "Active High", ISS_ON);
    ActiveStateSP.fill(getDeviceName(), "ACTIVE_STATE", "Active State", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Relay 1 switch
    Relay1SP[RELAY_OFF].fill("RELAY1_OFF", "OFF", ISS_ON);
    Relay1SP[RELAY_ON].fill("RELAY1_ON", "ON", ISS_OFF);
    Relay1SP.fill(getDeviceName(), "RELAY_1", "AstraDC1", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Relay 2 switch
    Relay2SP[RELAY_OFF].fill("RELAY2_OFF", "OFF", ISS_ON);
    Relay2SP[RELAY_ON].fill("RELAY2_ON", "ON", ISS_OFF);
    Relay2SP.fill(getDeviceName(), "RELAY_2", "AstraDC2", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Relay 3 switch
    Relay3SP[RELAY_OFF].fill("RELAY3_OFF", "OFF", ISS_ON);
    Relay3SP[RELAY_ON].fill("RELAY3_ON", "ON", ISS_OFF);
    Relay3SP.fill(getDeviceName(), "RELAY_3", "AstraDC3", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Power monitoring DC1
    PowerDC1NP[PWR_VOLTAGE].fill("DC1_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    PowerDC1NP[PWR_CURRENT].fill("DC1_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    PowerDC1NP[PWR_POWER].fill("DC1_POWER", "Power (W)", "%.2f", 0, 80, 0, 0);
    PowerDC1NP.fill(getDeviceName(), "POWER_DC1", "DC1 Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Power monitoring DC2
    PowerDC2NP[PWR_VOLTAGE].fill("DC2_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    PowerDC2NP[PWR_CURRENT].fill("DC2_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    PowerDC2NP[PWR_POWER].fill("DC2_POWER", "Power (W)", "%.2f", 0, 80, 0, 0);
    PowerDC2NP.fill(getDeviceName(), "POWER_DC2", "DC2 Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Power monitoring DC3
    PowerDC3NP[PWR_VOLTAGE].fill("DC3_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    PowerDC3NP[PWR_CURRENT].fill("DC3_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    PowerDC3NP[PWR_POWER].fill("DC3_POWER", "Power (W)", "%.2f", 0, 80, 0, 0);
    PowerDC3NP.fill(getDeviceName(), "POWER_DC3", "DC3 Power", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Total power
    TotalPowerNP[0].fill("TOTAL_CURRENT", "Total Current (A)", "%.2f", 0, 20, 0, 0);
    TotalPowerNP[1].fill("TOTAL_ENERGY", "Energy (Wh)", "%.2f", 0, 10000, 0, 0);
    TotalPowerNP.fill(getDeviceName(), "TOTAL_POWER", "Total", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Define options before connecting
    defineProperty(BCMPinsNP);
    defineProperty(ActiveStateSP);
    
    // Load saved configuration
    loadConfig();
    
    // Update active state from loaded config
    activeState = (ActiveStateSP[STATE_HIGH].getState() == ISS_ON) ? 1 : 0;
    
    // Initialize relay states to OFF
    for (int i = 0; i < 3; i++)
    {
        relayState[i] = !activeState;
    }
    
    addDebugControl();
    setDefaultPollingPeriod(POLLING_MS);
    
    return true;
}

bool AstrAlimRelays::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    
    if (isConnected())
    {
        defineProperty(Relay1SP);
        defineProperty(Relay2SP);
        defineProperty(Relay3SP);
        defineProperty(PowerDC1NP);
        defineProperty(PowerDC2NP);
        defineProperty(PowerDC3NP);
        defineProperty(TotalPowerNP);
    }
    else
    {
        deleteProperty(Relay1SP);
        deleteProperty(Relay2SP);
        deleteProperty(Relay3SP);
        deleteProperty(PowerDC1NP);
        deleteProperty(PowerDC2NP);
        deleteProperty(PowerDC3NP);
        deleteProperty(TotalPowerNP);
    }
    
    return true;
}

bool AstrAlimRelays::Connect()
{
    if (!gpio->openChip(AstrAlim::RPI5_GPIO_CHIP))
    {
        LOG_ERROR("Failed to open GPIO chip");
        return false;
    }
    
    // Get configured pins
    int pins[3] = {
        static_cast<int>(BCMPinsNP[0].getValue()),
        static_cast<int>(BCMPinsNP[1].getValue()),
        static_cast<int>(BCMPinsNP[2].getValue())
    };
    
    // Check if pins are available
    for (int i = 0; i < 3; i++)
    {
        if (gpio->isLineUsed(pins[i]))
        {
            std::string consumer = gpio->getLineConsumer(pins[i]);
            if (!consumer.empty())
            {
                LOGF_ERROR("GPIO pin %d (DC%d) is already in use by '%s'. "
                          "Please stop the Python script or other process using this GPIO before connecting.",
                          pins[i], i + 1, consumer.c_str());
            }
            else
            {
                LOGF_ERROR("GPIO pin %d (DC%d) is already in use by another process. "
                          "Please stop any Python scripts (AstraGpio.py) or other processes using this GPIO before connecting.",
                          pins[i], i + 1);
            }
            gpio->closeChip();
            return false;
        }
    }
    
    // Request GPIO lines - start with relays OFF
    // OFF = !activeState (if activeState=0/active-low, OFF means GPIO=1)
    for (int i = 0; i < 3; i++)
    {
        char consumer[32];
        snprintf(consumer, sizeof(consumer), "dc%d@astralim_relays", i + 1);
        
        // Start OFF: if activeState=0 (active low), GPIO should be 1 (inactive)
        // if activeState=1 (active high), GPIO should be 0 (inactive)
        int initialValue = !activeState;
        relayState[i] = initialValue;
        
        if (!gpio->requestOutput(pins[i], consumer, initialValue))
        {
            LOGF_ERROR("Failed to request GPIO pin %d", pins[i]);
            gpio->closeChip();
            return false;
        }
    }
    
    // Lock settings while connected
    BCMPinsNP.setState(IPS_BUSY);
    BCMPinsNP.apply();
    ActiveStateSP.setState(IPS_BUSY);
    ActiveStateSP.apply();
    
    // Reset energy counter
    totalEnergymWh = 0;
    
    // Start polling
    SetTimer(POLLING_MS);
    
    LOG_INFO("AstrAlim Relays connected successfully");
    return true;
}

bool AstrAlimRelays::Disconnect()
{
    gpio->closeChip();
    
    // Unlock settings
    BCMPinsNP.setState(IPS_IDLE);
    BCMPinsNP.apply();
    ActiveStateSP.setState(IPS_IDLE);
    ActiveStateSP.apply();
    
    LOG_INFO("AstrAlim Relays disconnected");
    return true;
}

void AstrAlimRelays::TimerHit()
{
    if (!isConnected())
        return;
    
    updateSwitchStates();
    readINA219();
    SetTimer(POLLING_MS);
}

void AstrAlimRelays::updateSwitchStates()
{
    // Relay 1
    int gpioValue = gpio->getValue(static_cast<int>(BCMPinsNP[0].getValue()));
    if (gpioValue >= 0)
    {
        int logicalState = (activeState == 0) ? !gpioValue : gpioValue;
        bool currentlyOn = (Relay1SP[RELAY_ON].getState() == ISS_ON);
        if (currentlyOn != (logicalState == 1))
        {
            Relay1SP.reset();
            Relay1SP[logicalState ? RELAY_ON : RELAY_OFF].setState(ISS_ON);
            Relay1SP.setState(logicalState ? IPS_OK : IPS_IDLE);
            Relay1SP.apply();
        }
    }
    
    // Relay 2
    gpioValue = gpio->getValue(static_cast<int>(BCMPinsNP[1].getValue()));
    if (gpioValue >= 0)
    {
        int logicalState = (activeState == 0) ? !gpioValue : gpioValue;
        bool currentlyOn = (Relay2SP[RELAY_ON].getState() == ISS_ON);
        if (currentlyOn != (logicalState == 1))
        {
            Relay2SP.reset();
            Relay2SP[logicalState ? RELAY_ON : RELAY_OFF].setState(ISS_ON);
            Relay2SP.setState(logicalState ? IPS_OK : IPS_IDLE);
            Relay2SP.apply();
        }
    }
    
    // Relay 3
    gpioValue = gpio->getValue(static_cast<int>(BCMPinsNP[2].getValue()));
    if (gpioValue >= 0)
    {
        int logicalState = (activeState == 0) ? !gpioValue : gpioValue;
        bool currentlyOn = (Relay3SP[RELAY_ON].getState() == ISS_ON);
        if (currentlyOn != (logicalState == 1))
        {
            Relay3SP.reset();
            Relay3SP[logicalState ? RELAY_ON : RELAY_OFF].setState(ISS_ON);
            Relay3SP.setState(logicalState ? IPS_OK : IPS_IDLE);
            Relay3SP.apply();
        }
    }
}

void AstrAlimRelays::readINA219()
{
    // Read INA219 sensors via Python helper
    // Format: voltage,current,power for each sensor
    std::string result = execCommand(
        "python3 -c \""
        "import sys\n"
        "import fcntl\n"
        "lockf = open('/tmp/astradiy_i2c.lock', 'w')\n"
        "fcntl.flock(lockf, fcntl.LOCK_EX)\n"
        "sys.path.insert(0, '/home/stellarmate')\n"
        "try:\n"
        "    from lib.ina219 import INA219\n"
        "    results = []\n"
        "    for addr in [0x41, 0x44, 0x46]:\n"
        "        try:\n"
        "            ina = INA219(0.01, 6, busnum=1, address=addr)\n"
        "            ina.configure()\n"
        "            v = max(ina.voltage(), 0)\n"
        "            c = max(ina.current()/1000, 0)\n"
        "            p = max(ina.power()/1000, 0)\n"
        "            results.append(f'{v:.3f},{c:.3f},{p:.3f}')\n"
        "        except:\n"
        "            results.append('0,0,0')\n"
        "    print('|'.join(results))\n"
        "except Exception as e:\n"
        "    print('0,0,0|0,0,0|0,0,0')\n"
        "\" 2>/dev/null"
    );
    
    if (result.empty())
    {
        PowerDC1NP.setState(IPS_ALERT);
        PowerDC2NP.setState(IPS_ALERT);
        PowerDC3NP.setState(IPS_ALERT);
        PowerDC1NP.apply();
        PowerDC2NP.apply();
        PowerDC3NP.apply();
        return;
    }
    
    // Parse result: v1,c1,p1|v2,c2,p2|v3,c3,p3
    double totalCurrent = 0;
    double totalPower = 0;
    
    std::istringstream iss(result);
    std::string sensorData;
    
    // DC1
    if (std::getline(iss, sensorData, '|'))
    {
        double v = 0, c = 0, p = 0;
        sscanf(sensorData.c_str(), "%lf,%lf,%lf", &v, &c, &p);
        PowerDC1NP[PWR_VOLTAGE].setValue(v);
        PowerDC1NP[PWR_CURRENT].setValue(c);
        PowerDC1NP[PWR_POWER].setValue(p);
        PowerDC1NP.setState((v > 0 || c > 0) ? IPS_OK : IPS_IDLE);
        PowerDC1NP.apply();
        totalCurrent += c;
        totalPower += p;
    }
    
    // DC2
    if (std::getline(iss, sensorData, '|'))
    {
        double v = 0, c = 0, p = 0;
        sscanf(sensorData.c_str(), "%lf,%lf,%lf", &v, &c, &p);
        PowerDC2NP[PWR_VOLTAGE].setValue(v);
        PowerDC2NP[PWR_CURRENT].setValue(c);
        PowerDC2NP[PWR_POWER].setValue(p);
        PowerDC2NP.setState((v > 0 || c > 0) ? IPS_OK : IPS_IDLE);
        PowerDC2NP.apply();
        totalCurrent += c;
        totalPower += p;
    }
    
    // DC3
    if (std::getline(iss, sensorData, '|'))
    {
        double v = 0, c = 0, p = 0;
        sscanf(sensorData.c_str(), "%lf,%lf,%lf", &v, &c, &p);
        PowerDC3NP[PWR_VOLTAGE].setValue(v);
        PowerDC3NP[PWR_CURRENT].setValue(c);
        PowerDC3NP[PWR_POWER].setValue(p);
        PowerDC3NP.setState((v > 0 || c > 0) ? IPS_OK : IPS_IDLE);
        PowerDC3NP.apply();
        totalCurrent += c;
        totalPower += p;
    }
    
    // Update total and accumulate energy
    TotalPowerNP[0].setValue(totalCurrent);
    
    // Accumulate energy (power in W * time in hours)
    double hoursElapsed = POLLING_MS / 3600000.0;
    totalEnergymWh += totalPower * hoursElapsed;
    TotalPowerNP[1].setValue(totalEnergymWh);
    
    TotalPowerNP.setState(IPS_OK);
    TotalPowerNP.apply();
}

std::string AstrAlimRelays::execCommand(const char* cmd)
{
    std::array<char, 256> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        return "";
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    
    // Remove trailing newline
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    
    return result;
}

bool AstrAlimRelays::setRelay(int relay, bool on)
{
    if (relay < 0 || relay > 2)
        return false;
    
    int pin = static_cast<int>(BCMPinsNP[relay].getValue());
    int gpioValue = on ? activeState : !activeState;
    
    if (!gpio->setValue(pin, gpioValue))
    {
        LOGF_ERROR("Failed to set relay %d", relay + 1);
        return false;
    }
    
    relayState[relay] = gpioValue;
    return true;
}

bool AstrAlimRelays::ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // BCM Pins
        if (BCMPinsNP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Cannot change BCM pins while connected");
                return false;
            }
            
            // Validate pins
            for (int i = 0; i < n; i++)
            {
                if (values[i] < 1 || values[i] > 27)
                {
                    LOGF_ERROR("Invalid BCM pin: %.0f", values[i]);
                    BCMPinsNP.setState(IPS_ALERT);
                    BCMPinsNP.apply();
                    return false;
                }
                
                // Check for duplicates
                for (int j = i + 1; j < n; j++)
                {
                    if (values[i] == values[j])
                    {
                        LOG_ERROR("Duplicate BCM pin assignment");
                        BCMPinsNP.setState(IPS_ALERT);
                        BCMPinsNP.apply();
                        return false;
                    }
                }
            }
            
            BCMPinsNP.update(values, names, n);
            BCMPinsNP.setState(IPS_OK);
            BCMPinsNP.apply();
            
            LOGF_INFO("BCM Pins set to DC1: %.0f, DC2: %.0f, DC3: %.0f",
                      BCMPinsNP[0].getValue(), BCMPinsNP[1].getValue(), BCMPinsNP[2].getValue());
            return true;
        }
    }
    
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool AstrAlimRelays::ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Active state
        if (ActiveStateSP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Cannot change active state while connected");
                return false;
            }
            
            ActiveStateSP.update(states, names, n);
            activeState = (ActiveStateSP[STATE_HIGH].getState() == ISS_ON) ? 1 : 0;
            ActiveStateSP.setState(IPS_OK);
            ActiveStateSP.apply();
            
            LOGF_INFO("Active state set to %s", activeState ? "HIGH" : "LOW");
            return true;
        }
        
        // Relay 1
        if (Relay1SP.isNameMatch(name))
        {
            bool turnOn = (strcmp(names[0], "RELAY1_ON") == 0 && states[0] == ISS_ON);
            
            if (!setRelay(0, turnOn))
            {
                Relay1SP.setState(IPS_ALERT);
                Relay1SP.apply();
                return false;
            }
            
            Relay1SP.update(states, names, n);
            Relay1SP.setState(turnOn ? IPS_OK : IPS_IDLE);
            Relay1SP.apply();
            
            LOGF_INFO("Relay DC1 set to %s", turnOn ? "ON" : "OFF");
            return true;
        }
        
        // Relay 2
        if (Relay2SP.isNameMatch(name))
        {
            bool turnOn = (strcmp(names[0], "RELAY2_ON") == 0 && states[0] == ISS_ON);
            
            if (!setRelay(1, turnOn))
            {
                Relay2SP.setState(IPS_ALERT);
                Relay2SP.apply();
                return false;
            }
            
            Relay2SP.update(states, names, n);
            Relay2SP.setState(turnOn ? IPS_OK : IPS_IDLE);
            Relay2SP.apply();
            
            LOGF_INFO("Relay DC2 set to %s", turnOn ? "ON" : "OFF");
            return true;
        }
        
        // Relay 3
        if (Relay3SP.isNameMatch(name))
        {
            bool turnOn = (strcmp(names[0], "RELAY3_ON") == 0 && states[0] == ISS_ON);
            
            if (!setRelay(2, turnOn))
            {
                Relay3SP.setState(IPS_ALERT);
                Relay3SP.apply();
                return false;
            }
            
            Relay3SP.update(states, names, n);
            Relay3SP.setState(turnOn ? IPS_OK : IPS_IDLE);
            Relay3SP.apply();
            
            LOGF_INFO("Relay DC3 set to %s", turnOn ? "ON" : "OFF");
            return true;
        }
    }
    
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AstrAlimRelays::saveConfigItems(FILE* fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    
    BCMPinsNP.save(fp);
    ActiveStateSP.save(fp);
    Relay1SP.save(fp);
    Relay2SP.save(fp);
    Relay3SP.save(fp);
    
    return true;
}
