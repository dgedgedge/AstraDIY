/*******************************************************************************
 * AstrAlim Heater Driver - Dew Heater Controller
 * Copyright (c) 2024 AstrAlim Project
 ******************************************************************************/

#include "astralim_heater.h"
#include "astralim_gpio.h"
#include "astralim_astra_ina.h"
#include "astralim_bme_fetcher.h"
#include "astralim_step_pwm_actor.h"
#include "astralim_com_fetcher.h"
#include "config.h"

#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <array>
#include <vector>
#include <algorithm>
#include <chrono>
#include <set>

namespace
{
constexpr const char* RELAYS_TAB = "Relays";
constexpr const char* DEW_POINT_TAB = "Dew Point";
constexpr const char* ENERGY_MONITOR_TAB = "Energy Monitor";
}

// Singleton instance
static std::unique_ptr<AstrAlimHeater> heaterInstance(new AstrAlimHeater());

AstrAlimHeater::AstrAlimHeater()
{
    setVersion(INDI_ASTRALIM_VERSION_MAJOR, INDI_ASTRALIM_VERSION_MINOR);
    pidRunning[0] = false;
    pidRunning[1] = false;
    pwmGpio = std::make_unique<AstrAlim::GpioController>();
    
    // Initialiser les états d'assignation
    for (int i = 0; i < 2; i++)
    {
        sensorAssignState[i].autoDetectActive = false;
        sensorAssignState[i].testPower = 50;  // 50% par défaut pour le test
        for (int j = 0; j < 10; j++)
        {
            sensorAssignState[i].testStartTemp[j] = TEMP_UNAVAILABLE;
        }
    }
}


AstrAlimHeater::~AstrAlimHeater()
{
    // Stop PID threads
    for (int i = 0; i < 2; i++)
    {
        pidRunning[i] = false;
        if (pidThread[i].joinable())
            pidThread[i].join();
    }
    closePWM();
}

const char* AstrAlimHeater::getDefaultName()
{
    return "AstrAlim Heater";
}

bool AstrAlimHeater::initProperties()
{
    INDI::DefaultDevice::initProperties();
    
    // ===== Ambient Sensor (BME280/BMP280) =====
    AmbientNP[AMB_TEMPERATURE].fill("AMBIENT_TEMP", "Temperature (°C)", "%.1f", -40, 85, 0.1, 0);
    AmbientNP[AMB_HUMIDITY].fill("AMBIENT_HUMIDITY", "Humidity (%)", "%.1f", 0, 100, 0.1, 0);
    AmbientNP[AMB_PRESSURE].fill("AMBIENT_PRESSURE", "Pressure (hPa)", "%.1f", 300, 1100, 0.1, 0);
    AmbientNP[AMB_DEWPOINT].fill("DEW_POINT", "Dew Point (°C)", "%.1f", -40, 85, 0.1, DEWPOINT_UNAVAILABLE);
    AmbientNP.fill(getDeviceName(), "AMBIENT_SENSOR", "Ambient", DEW_POINT_TAB, IP_RO, 60, IPS_IDLE);
    
    // Manual humidity (for BMP280)
    ManualHumidityNP[0].fill("MANUAL_HUMIDITY", "Humidity (%)", "%.0f", 0, 100, 5, 50);
    ManualHumidityNP.fill(getDeviceName(), "MANUAL_HUMIDITY", "Manual Humidity", DEW_POINT_TAB, IP_RW, 60, IPS_IDLE);
    
    // ===== Heater 1 =====
    // Initialiser à 0 au lieu de TEMP_UNAVAILABLE pour éviter d'afficher 100°C au démarrage
    // La valeur sera mise à jour lors de la première lecture dans Connect()
    Heater1TempNP[0].fill("HEATER1_TEMP", "Temperature (°C)", "%.1f", -40, 125, 0.1, 0);
    Heater1TempNP.fill(getDeviceName(), "HEATER1_TEMPERATURE", "Heater 1 Temp", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    Heater1PowerNP[0].fill("HEATER1_POWER", "Power (%)", "%.0f", 0, 100, 5, 0);
    Heater1PowerNP.fill(getDeviceName(), "HEATER1_POWER", "Heater 1 Power", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    
    Heater1SetpointNP[0].fill("HEATER1_SETPOINT", "Setpoint (°C)", "%.1f", -10, 40, 0.5, 10);
    Heater1SetpointNP.fill(getDeviceName(), "HEATER1_SETPOINT", "Heater 1 Target", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    
    Heater1ModeSP[MODE_OFF].fill("HEATER1_OFF", "Off", ISS_ON);
    Heater1ModeSP[MODE_POWER].fill("HEATER1_POWER_MODE", "Power", ISS_OFF);
    Heater1ModeSP[MODE_SETPOINT].fill("HEATER1_SETPOINT_MODE", "Setpoint", ISS_OFF);
    Heater1ModeSP[MODE_AUTO_DEW].fill("HEATER1_AUTO_DEW", "Auto Dew", ISS_OFF);
    Heater1ModeSP.fill(getDeviceName(), "HEATER1_MODE", "Heater 1 Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    Heater1SensorTP[0].fill("HEATER1_SENSOR_ID", "Sensor ID", "");
    Heater1SensorTP.fill(getDeviceName(), "HEATER1_SENSOR", "Heater 1 Sensor", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    Heater1SensorAssignSP[SENSOR_ASSIGN_AUTO].fill("HEATER1_SENSOR_AUTO", "Auto-Detect", ISS_OFF);
    Heater1SensorAssignSP[SENSOR_ASSIGN_TEST].fill("HEATER1_SENSOR_TEST", "Test Response", ISS_OFF);
    Heater1SensorAssignSP[SENSOR_ASSIGN_CLEAR].fill("HEATER1_SENSOR_CLEAR", "Clear", ISS_OFF);
    Heater1SensorAssignSP.fill(getDeviceName(), "HEATER1_SENSOR_ASSIGN", "Heater 1 Sensor Assignment", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    
    // ===== Heater 2 =====
    // Initialiser à 0 au lieu de TEMP_UNAVAILABLE pour éviter d'afficher 100°C au démarrage
    // La valeur sera mise à jour lors de la première lecture dans Connect()
    Heater2TempNP[0].fill("HEATER2_TEMP", "Temperature (°C)", "%.1f", -40, 125, 0.1, 0);
    Heater2TempNP.fill(getDeviceName(), "HEATER2_TEMPERATURE", "Heater 2 Temp", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    Heater2PowerNP[0].fill("HEATER2_POWER", "Power (%)", "%.0f", 0, 100, 5, 0);
    Heater2PowerNP.fill(getDeviceName(), "HEATER2_POWER", "Heater 2 Power", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    
    Heater2SetpointNP[0].fill("HEATER2_SETPOINT", "Setpoint (°C)", "%.1f", -10, 40, 0.5, 10);
    Heater2SetpointNP.fill(getDeviceName(), "HEATER2_SETPOINT", "Heater 2 Target", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    
    Heater2ModeSP[MODE_OFF].fill("HEATER2_OFF", "Off", ISS_ON);
    Heater2ModeSP[MODE_POWER].fill("HEATER2_POWER_MODE", "Power", ISS_OFF);
    Heater2ModeSP[MODE_SETPOINT].fill("HEATER2_SETPOINT_MODE", "Setpoint", ISS_OFF);
    Heater2ModeSP[MODE_AUTO_DEW].fill("HEATER2_AUTO_DEW", "Auto Dew", ISS_OFF);
    Heater2ModeSP.fill(getDeviceName(), "HEATER2_MODE", "Heater 2 Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    Heater2SensorTP[0].fill("HEATER2_SENSOR_ID", "Sensor ID", "");
    Heater2SensorTP.fill(getDeviceName(), "HEATER2_SENSOR", "Heater 2 Sensor", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    Heater2SensorAssignSP[SENSOR_ASSIGN_AUTO].fill("HEATER2_SENSOR_AUTO", "Auto-Detect", ISS_OFF);
    Heater2SensorAssignSP[SENSOR_ASSIGN_TEST].fill("HEATER2_SENSOR_TEST", "Test Response", ISS_OFF);
    Heater2SensorAssignSP[SENSOR_ASSIGN_CLEAR].fill("HEATER2_SENSOR_CLEAR", "Clear", ISS_OFF);
    Heater2SensorAssignSP.fill(getDeviceName(), "HEATER2_SENSOR_ASSIGN", "Heater 2 Sensor Assignment", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    
    // ===== Available Sensors =====
    // Ne pas initialiser ici - sera créé dynamiquement dans updateSensorStatusList()
    // La propriété sera définie après le scan des capteurs dans Connect()
    
    // ===== Sensor Assignment Actions =====
    SensorAssignActionSP[ASSIGN_TO_HEATER1].fill("ASSIGN_TO_H1", "Assign to Heater 1", ISS_OFF);
    SensorAssignActionSP[ASSIGN_TO_HEATER2].fill("ASSIGN_TO_H2", "Assign to Heater 2", ISS_OFF);
    SensorAssignActionSP.fill(getDeviceName(), "SENSOR_ASSIGN_ACTION", "Assign Selected Sensor", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    
    // ===== PID Parameters =====
    PIDNP[PID_KP].fill("PID_KP", "Kp", "%.2f", 0, 10, 0.1, DEFAULT_KP);
    PIDNP[PID_KI].fill("PID_KI", "Ki", "%.3f", 0, 1, 0.01, DEFAULT_KI);
    PIDNP[PID_KD].fill("PID_KD", "Kd", "%.2f", 0, 5, 0.1, DEFAULT_KD);
    PIDNP.fill(getDeviceName(), "PID_PARAMS", "PID Parameters", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // ===== Dew Delta =====
    DewDeltaNP[0].fill("DEW_DELTA", "Delta (°C)", "%.1f", 0, 20, 0.5, DEFAULT_DEW_DELTA);
    DewDeltaNP.fill(getDeviceName(), "DEW_DELTA", "Dew Point Delta", DEW_POINT_TAB, IP_RW, 60, IPS_IDLE);
    
    // ===== Relay Controls =====
    BCMPinsNP[0].fill("BCMPIN_DC1", "DC1", "%0.0f", 1, 27, 0, AstrAlim::RelayPins::DC1);
    BCMPinsNP[1].fill("BCMPIN_DC2", "DC2", "%0.0f", 1, 27, 0, AstrAlim::RelayPins::DC2);
    BCMPinsNP[2].fill("BCMPIN_DC3", "DC3", "%0.0f", 1, 27, 0, AstrAlim::RelayPins::DC3);
    BCMPinsNP.fill(getDeviceName(), "BCMPINS", "BCM Pins", RELAYS_TAB, IP_RO, 60, IPS_IDLE);

    ActiveStateSP[RELAY_STATE_LOW].fill("ACTIVE_LOW", "Active Low", ISS_OFF);
    ActiveStateSP[RELAY_STATE_HIGH].fill("ACTIVE_HIGH", "Active High", ISS_ON);
    ActiveStateSP.fill(getDeviceName(), "ACTIVE_STATE", "Active State", RELAYS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    Relay1SP[RELAY_SW_OFF].fill("RELAY1_OFF", "OFF", ISS_ON);
    Relay1SP[RELAY_SW_ON].fill("RELAY1_ON", "ON", ISS_OFF);
    Relay1SP.fill(getDeviceName(), "RELAY_1", "AstraDC1", RELAYS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    Relay2SP[RELAY_SW_OFF].fill("RELAY2_OFF", "OFF", ISS_ON);
    Relay2SP[RELAY_SW_ON].fill("RELAY2_ON", "ON", ISS_OFF);
    Relay2SP.fill(getDeviceName(), "RELAY_2", "AstraDC2", RELAYS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    Relay3SP[RELAY_SW_OFF].fill("RELAY3_OFF", "OFF", ISS_ON);
    Relay3SP[RELAY_SW_ON].fill("RELAY3_ON", "ON", ISS_OFF);
    Relay3SP.fill(getDeviceName(), "RELAY_3", "AstraDC3", RELAYS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // ===== INA Monitoring (Heaters + Relays) =====
    HeaterPower1NP[HEATER_PWR_VOLTAGE].fill("HEATER1_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    HeaterPower1NP[HEATER_PWR_CURRENT].fill("HEATER1_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    HeaterPower1NP[HEATER_PWR_POWER].fill("HEATER1_POWER_W", "Power (W)", "%.2f", 0, 80, 0, 0);
    HeaterPower1NP.fill(getDeviceName(), "HEATER_POWER_1", "Heater 1", ENERGY_MONITOR_TAB, IP_RO, 60, IPS_IDLE);

    HeaterPower2NP[HEATER_PWR_VOLTAGE].fill("HEATER2_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    HeaterPower2NP[HEATER_PWR_CURRENT].fill("HEATER2_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    HeaterPower2NP[HEATER_PWR_POWER].fill("HEATER2_POWER_W", "Power (W)", "%.2f", 0, 80, 0, 0);
    HeaterPower2NP.fill(getDeviceName(), "HEATER_POWER_2", "Heater 2", ENERGY_MONITOR_TAB, IP_RO, 60, IPS_IDLE);

    RelayPowerDC1NP[RELAY_PWR_VOLTAGE].fill("DC1_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    RelayPowerDC1NP[RELAY_PWR_CURRENT].fill("DC1_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    RelayPowerDC1NP[RELAY_PWR_POWER].fill("DC1_POWER", "Power (W)", "%.2f", 0, 80, 0, 0);
    RelayPowerDC1NP.fill(getDeviceName(), "RELAY_POWER_DC1", "Relay DC1", ENERGY_MONITOR_TAB, IP_RO, 60, IPS_IDLE);

    RelayPowerDC2NP[RELAY_PWR_VOLTAGE].fill("DC2_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    RelayPowerDC2NP[RELAY_PWR_CURRENT].fill("DC2_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    RelayPowerDC2NP[RELAY_PWR_POWER].fill("DC2_POWER", "Power (W)", "%.2f", 0, 80, 0, 0);
    RelayPowerDC2NP.fill(getDeviceName(), "RELAY_POWER_DC2", "Relay DC2", ENERGY_MONITOR_TAB, IP_RO, 60, IPS_IDLE);

    RelayPowerDC3NP[RELAY_PWR_VOLTAGE].fill("DC3_VOLTAGE", "Voltage (V)", "%.2f", 0, 15, 0, 0);
    RelayPowerDC3NP[RELAY_PWR_CURRENT].fill("DC3_CURRENT", "Current (A)", "%.3f", 0, 6, 0, 0);
    RelayPowerDC3NP[RELAY_PWR_POWER].fill("DC3_POWER", "Power (W)", "%.2f", 0, 80, 0, 0);
    RelayPowerDC3NP.fill(getDeviceName(), "RELAY_POWER_DC3", "Relay DC3", ENERGY_MONITOR_TAB, IP_RO, 60, IPS_IDLE);

    RelayTotalPowerNP[0].fill("RELAY_TOTAL_CURRENT", "Total Current (A)", "%.2f", 0, 20, 0, 0);
    RelayTotalPowerNP[1].fill("RELAY_TOTAL_ENERGY", "Energy (Wh)", "%.2f", 0, 10000, 0, 0);
    RelayTotalPowerNP.fill(getDeviceName(), "RELAY_TOTAL_POWER", "Relay Total", ENERGY_MONITOR_TAB, IP_RO, 60, IPS_IDLE);
    
    addDebugControl();
    setDefaultPollingPeriod(POLL_INTERVAL_MS);
    
    return true;
}

bool AstrAlimHeater::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    
    if (isConnected())
    {
        // ---- Main Control Tab ----
        defineProperty(Heater1TempNP);
        defineProperty(Heater1PowerNP);
        defineProperty(Heater1SetpointNP);
        defineProperty(Heater1ModeSP);
        defineProperty(Heater2TempNP);
        defineProperty(Heater2PowerNP);
        defineProperty(Heater2SetpointNP);
        defineProperty(Heater2ModeSP);
        // ---- Options Tab ----
        defineProperty(Heater1SensorTP);
        defineProperty(Heater1SensorAssignSP);
        defineProperty(Heater2SensorTP);
        defineProperty(Heater2SensorAssignSP);
        // AvailableSensorsSP sera défini dynamiquement après le scan
        if (AvailableSensorsSP.size() > 0)
        {
            defineProperty(AvailableSensorsSP);
        }
        defineProperty(SensorAssignActionSP);
        defineProperty(PIDNP);
        // ---- Dew Point Tab ----
        defineProperty(AmbientNP);
        defineProperty(ManualHumidityNP);
        defineProperty(DewDeltaNP);
        // ---- Relays Tab ----
        defineProperty(BCMPinsNP);
        defineProperty(ActiveStateSP);
        defineProperty(Relay1SP);
        defineProperty(Relay2SP);
        defineProperty(Relay3SP);
        // ---- Energy Monitor Tab ----
        defineProperty(HeaterPower1NP);
        defineProperty(HeaterPower2NP);
        defineProperty(RelayPowerDC1NP);
        defineProperty(RelayPowerDC2NP);
        defineProperty(RelayPowerDC3NP);
        defineProperty(RelayTotalPowerNP);

    }
    else
    {
        deleteProperty(Heater1TempNP);
        deleteProperty(Heater1PowerNP);
        deleteProperty(Heater1SetpointNP);
        deleteProperty(Heater1ModeSP);
        deleteProperty(Heater2TempNP);
        deleteProperty(Heater2PowerNP);
        deleteProperty(Heater2SetpointNP);
        deleteProperty(Heater2ModeSP);
        deleteProperty(Heater1SensorTP);
        deleteProperty(Heater1SensorAssignSP);
        deleteProperty(Heater2SensorTP);
        deleteProperty(Heater2SensorAssignSP);
        if (AvailableSensorsSP.size() > 0)
        {
            deleteProperty(AvailableSensorsSP);
        }
        deleteProperty(SensorAssignActionSP);
        deleteProperty(PIDNP);
        deleteProperty(AmbientNP);
        deleteProperty(ManualHumidityNP);
        deleteProperty(DewDeltaNP);
        deleteProperty(BCMPinsNP);
        deleteProperty(ActiveStateSP);
        deleteProperty(Relay1SP);
        deleteProperty(Relay2SP);
        deleteProperty(Relay3SP);
        deleteProperty(HeaterPower1NP);
        deleteProperty(HeaterPower2NP);
        deleteProperty(RelayPowerDC1NP);
        deleteProperty(RelayPowerDC2NP);
        deleteProperty(RelayPowerDC3NP);
        deleteProperty(RelayTotalPowerNP);

    }
    
    return true;
}

bool AstrAlimHeater::Connect()
{
    // Initialize PWM
    if (!initPWM())
    {
        LOG_ERROR("Failed to initialize PWM. Check permissions and PWM overlay.");
        return false;
    }

    if (!initRelays())
    {
        closePWM();
        LOG_ERROR("Failed to initialize relay GPIOs.");
        return false;
    }
    relayTotalEnergyWh = 0.0;
    
    // Scan for DS18B20 sensors
    availableDS18B20 = scanDS18B20Devices();
    
    LOGF_INFO("Found %zu DS18B20 sensor(s)", availableDS18B20.size());
    
    // Mettre à jour la liste des capteurs avec statuts
    updateSensorStatusList();
    
    // Auto-assign sensors if available (seulement si pas de config sauvegardée)
    if (availableDS18B20.size() >= 1 && strlen(Heater1SensorTP[0].getText()) == 0)
    {
        Heater1SensorTP[0].setText(availableDS18B20[0].c_str());
        ds18b20Path[0] = "/sys/bus/w1/devices/" + availableDS18B20[0] + "/w1_slave";
        Heater1SensorTP.setState(IPS_OK);
        Heater1SensorTP.apply();
        LOGF_INFO("Auto-assigned sensor %s to Heater 1", availableDS18B20[0].c_str());
    }
    if (availableDS18B20.size() >= 2 && strlen(Heater2SensorTP[0].getText()) == 0)
    {
        Heater2SensorTP[0].setText(availableDS18B20[1].c_str());
        ds18b20Path[1] = "/sys/bus/w1/devices/" + availableDS18B20[1] + "/w1_slave";
        Heater2SensorTP.setState(IPS_OK);
        Heater2SensorTP.apply();
        LOGF_INFO("Auto-assigned sensor %s to Heater 2", availableDS18B20[1].c_str());
    }
    
    // Valider les capteurs assignés
    validateAssignedSensors();
    
    // Security: Force OFF mode on startup (even if saved config has different mode)
    Heater1ModeSP[MODE_OFF].setState(ISS_ON);
    Heater1ModeSP[MODE_POWER].setState(ISS_OFF);
    Heater1ModeSP[MODE_SETPOINT].setState(ISS_OFF);
    Heater1ModeSP[MODE_AUTO_DEW].setState(ISS_OFF);
    Heater1ModeSP.setState(IPS_IDLE);
    Heater1ModeSP.apply();
    
    Heater2ModeSP[MODE_OFF].setState(ISS_ON);
    Heater2ModeSP[MODE_POWER].setState(ISS_OFF);
    Heater2ModeSP[MODE_SETPOINT].setState(ISS_OFF);
    Heater2ModeSP[MODE_AUTO_DEW].setState(ISS_OFF);
    Heater2ModeSP.setState(IPS_IDLE);
    Heater2ModeSP.apply();
    
    // Ensure heaters are off
    setPWMDuty(0, 0);
    setPWMDuty(1, 0);
    Heater1PowerNP[0].setValue(0);
    Heater2PowerNP[0].setValue(0);
    Heater1PowerNP.setState(IPS_IDLE);
    Heater2PowerNP.setState(IPS_IDLE);
    Heater1PowerNP.apply();
    Heater2PowerNP.apply();
    
    // Initial readings
    resetINADisplayState();
    readBME280();
    readDS18B20Sensors();
    
    SetTimer(POLL_INTERVAL_MS);
    
    LOG_INFO("AstrAlim Heater connected successfully (heaters OFF)");
    return true;
}

bool AstrAlimHeater::Disconnect()
{
    // Stop PID threads
    for (int i = 0; i < 2; i++)
    {
        pidRunning[i] = false;
        if (pidThread[i].joinable())
            pidThread[i].join();
    }
    
    // Turn off heaters
    setPWMDuty(0, 0);
    setPWMDuty(1, 0);

    shutdownRelays();
    
    closePWM();
    resetINADisplayState();
    
    LOG_INFO("AstrAlim Heater disconnected");
    return true;
}

void AstrAlimHeater::TimerHit()
{
    if (!isConnected())
        return;

    // Reprogram immediately to reduce drift when a cycle contains slow I/O calls.
    SetTimer(POLL_INTERVAL_MS);
    
    // Read heater probe temperatures first so displayed values stay responsive.
    readDS18B20Sensors();
    readBME280();
    readINA219();
    updateRelaySwitchStates();
    updateSensorStatusList(false);
    
    // Mettre à jour la liste des capteurs avec statuts moins fréquemment
    // pour éviter de surcharger le bus 1-Wire.
    static int sensorListUpdateCounter = 0;
    if (++sensorListUpdateCounter >= SENSOR_LIST_UPDATE_INTERVAL_CYCLES)
    {
        updateSensorStatusList(true);
        validateAssignedSensors();
        sensorListUpdateCounter = 0;
    }
    
    // Gérer les tests de détection automatique en cours
    for (int ch = 0; ch < 2; ch++)
    {
        if (sensorAssignState[ch].autoDetectActive)
        {
            handleAutoDetect(ch);
        }
    }
    
    // Handle heater modes
    for (int ch = 0; ch < 2; ch++)
    {
        INDI::PropertySwitch& modeSP = (ch == 0) ? Heater1ModeSP : Heater2ModeSP;
        INDI::PropertyNumber& powerNP = (ch == 0) ? Heater1PowerNP : Heater2PowerNP;
        INDI::PropertyNumber& setpointNP = (ch == 0) ? Heater1SetpointNP : Heater2SetpointNP;
        INDI::PropertyNumber& tempNP = (ch == 0) ? Heater1TempNP : Heater2TempNP;
        
        if (modeSP[MODE_OFF].getState() == ISS_ON)
        {
            // Off mode
            if (pidRunning[ch])
            {
                pidRunning[ch] = false;
                if (pidThread[ch].joinable())
                    pidThread[ch].join();
            }
            setPWMDuty(ch, 0);
            powerNP[0].setValue(0);
            powerNP.setState(IPS_IDLE);
        }
        else if (modeSP[MODE_POWER].getState() == ISS_ON)
        {
            // Power mode - use power directly
            if (pidRunning[ch])
            {
                pidRunning[ch] = false;
                if (pidThread[ch].joinable())
                    pidThread[ch].join();
            }
            setPWMDuty(ch, powerNP[0].getValue());
            powerNP.setState(IPS_OK);
        }
        else if (modeSP[MODE_SETPOINT].getState() == ISS_ON)
        {
            // Setpoint mode - fixed setpoint with PID
            double targetTemp = setpointNP[0].getValue();
            double currentTemp = tempNP[0].getValue();

            if (currentTemp < TEMP_UNAVAILABLE - 10)
            {
                double output = computePID(ch, targetTemp, currentTemp);
                output = std::max(0.0, std::min(100.0, output));
                setPWMDuty(ch, output);
                powerNP[0].setValue(output);
                powerNP.setState(IPS_BUSY);
            }
            else
            {
                setPWMDuty(ch, 0);
                powerNP[0].setValue(0);
                powerNP.setState(IPS_ALERT);
            }
        }
        else if (modeSP[MODE_AUTO_DEW].getState() == ISS_ON)
        {
            // Auto Dew mode - setpoint computed from dew point, then PID
            // Utiliser le point de rosée filtré pour éviter les variations
            const double dewPoint = AstrAlim::AstraBmeFetcher::getInstance().getFilteredDewPoint();
            
            double targetTemp;
            
            if (dewPoint > DEWPOINT_UNAVAILABLE + 10)
            {
                targetTemp = dewPoint + DewDeltaNP[0].getValue();
                // Arrondir la consigne à 0.1°C près pour éviter les variations d'affichage
                targetTemp = std::round(targetTemp * 10.0) / 10.0;
            }
            else
            {
                // No dew point available, use setpoint directly
                targetTemp = setpointNP[0].getValue();
            }
            
            double currentTemp = tempNP[0].getValue();
            
            if (currentTemp < TEMP_UNAVAILABLE - 10)
            {
                // Valid temperature reading
                double output = computePID(ch, targetTemp, currentTemp);
                output = std::max(0.0, std::min(100.0, output));
                setPWMDuty(ch, output);
                powerNP[0].setValue(output);
                powerNP.setState(IPS_BUSY);
                setpointNP[0].setValue(targetTemp);
            }
            else
            {
                // No temperature reading, turn off
                setPWMDuty(ch, 0);
                powerNP[0].setValue(0);
                powerNP.setState(IPS_ALERT);
            }
        }
        
        powerNP.apply();
    }
    
    Heater1SetpointNP.apply();
    Heater2SetpointNP.apply();
}

// ==================== PWM Control ====================

bool AstrAlimHeater::initPWM()
{
    try
    {
        auto& fetcher = AstrAlim::AstraComFetcher::getInstance();
        fetcher.setCyclePeriod(static_cast<double>(STEP_PWM_TICK_MS) / 1000.0);
        fetcher.setCycleStepCount(STEP_PWM_STEP_COUNT);

        pwmActors[0] = std::make_unique<AstrAlim::AstraStepPwmActor>(STEP_PWM_GPIO_H1, 0.0, "Heater1StepPwm");
        pwmActors[1] = std::make_unique<AstrAlim::AstraStepPwmActor>(STEP_PWM_GPIO_H2, 0.0, "Heater2StepPwm");
    }
    catch (const std::exception& e)
    {
        LOGF_ERROR("Failed to initialize step PWM actors: %s", e.what());
        closePWM();
        return false;
    }

    LOGF_INFO("Step PWM actors started: tick=%dms steps=%d (Heater1 GPIO%d, Heater2 GPIO%d)",
              STEP_PWM_TICK_MS, STEP_PWM_STEP_COUNT, STEP_PWM_GPIO_H1, STEP_PWM_GPIO_H2);

    return true;
}

void AstrAlimHeater::closePWM()
{
    if (pwmActors[0])
        pwmActors[0].reset();
    if (pwmActors[1])
        pwmActors[1].reset();

    if (pwmGpio && pwmGpio->isOpen())
    {
        pwmGpio->closeChip();
    }
}

bool AstrAlimHeater::initRelays()
{
    if (!pwmGpio->isOpen() && !pwmGpio->openChip(AstrAlim::RPI5_GPIO_CHIP))
    {
        LOGF_ERROR("Failed to open GPIO chip %s for relays", AstrAlim::RPI5_GPIO_CHIP);
        return false;
    }

    activeState = (ActiveStateSP[RELAY_STATE_HIGH].getState() == ISS_ON) ? 1 : 0;

    const int pins[3] = {
        static_cast<int>(BCMPinsNP[0].getValue()),
        static_cast<int>(BCMPinsNP[1].getValue()),
        static_cast<int>(BCMPinsNP[2].getValue())
    };

    for (int i = 0; i < 3; i++)
    {
        if (pwmGpio->isLineUsed(pins[i]))
        {
            std::string consumer = pwmGpio->getLineConsumer(pins[i]);
            if (!consumer.empty())
            {
                LOGF_ERROR("GPIO pin %d (DC%d) is already in use by '%s'", pins[i], i + 1, consumer.c_str());
            }
            else
            {
                LOGF_ERROR("GPIO pin %d (DC%d) is already in use", pins[i], i + 1);
            }
            return false;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        char consumer[32];
        snprintf(consumer, sizeof(consumer), "dc%d@astralim_heater", i + 1);
        const int initialValue = !activeState;
        if (!pwmGpio->requestOutput(pins[i], consumer, initialValue))
        {
            LOGF_ERROR("Failed to request GPIO pin %d for relay DC%d", pins[i], i + 1);
            return false;
        }
        relayGpioState[i] = initialValue;
    }

    Relay1SP[RELAY_SW_OFF].setState(ISS_ON);
    Relay1SP[RELAY_SW_ON].setState(ISS_OFF);
    Relay1SP.setState(IPS_IDLE);
    Relay1SP.apply();

    Relay2SP[RELAY_SW_OFF].setState(ISS_ON);
    Relay2SP[RELAY_SW_ON].setState(ISS_OFF);
    Relay2SP.setState(IPS_IDLE);
    Relay2SP.apply();

    Relay3SP[RELAY_SW_OFF].setState(ISS_ON);
    Relay3SP[RELAY_SW_ON].setState(ISS_OFF);
    Relay3SP.setState(IPS_IDLE);
    Relay3SP.apply();

    return true;
}

void AstrAlimHeater::shutdownRelays()
{
    setRelay(0, false);
    setRelay(1, false);
    setRelay(2, false);
}

bool AstrAlimHeater::setRelay(int relayIndex, bool on)
{
    if (relayIndex < 0 || relayIndex > 2)
        return false;

    const int pin = static_cast<int>(BCMPinsNP[relayIndex].getValue());
    const int gpioValue = on ? activeState : !activeState;

    if (!pwmGpio->setValue(pin, gpioValue))
        return false;

    relayGpioState[relayIndex] = gpioValue;
    return true;
}

void AstrAlimHeater::updateRelaySwitchStates()
{
    const int pins[3] = {
        static_cast<int>(BCMPinsNP[0].getValue()),
        static_cast<int>(BCMPinsNP[1].getValue()),
        static_cast<int>(BCMPinsNP[2].getValue())
    };

    int gpioValue = pwmGpio->getValue(pins[0]);
    if (gpioValue >= 0)
    {
        const bool logicalOn = (activeState == 0) ? (gpioValue == 0) : (gpioValue == 1);
        const bool currentlyOn = (Relay1SP[RELAY_SW_ON].getState() == ISS_ON);
        if (logicalOn != currentlyOn)
        {
            Relay1SP.reset();
            Relay1SP[logicalOn ? RELAY_SW_ON : RELAY_SW_OFF].setState(ISS_ON);
            Relay1SP.setState(logicalOn ? IPS_OK : IPS_IDLE);
            Relay1SP.apply();
        }
    }

    gpioValue = pwmGpio->getValue(pins[1]);
    if (gpioValue >= 0)
    {
        const bool logicalOn = (activeState == 0) ? (gpioValue == 0) : (gpioValue == 1);
        const bool currentlyOn = (Relay2SP[RELAY_SW_ON].getState() == ISS_ON);
        if (logicalOn != currentlyOn)
        {
            Relay2SP.reset();
            Relay2SP[logicalOn ? RELAY_SW_ON : RELAY_SW_OFF].setState(ISS_ON);
            Relay2SP.setState(logicalOn ? IPS_OK : IPS_IDLE);
            Relay2SP.apply();
        }
    }

    gpioValue = pwmGpio->getValue(pins[2]);
    if (gpioValue >= 0)
    {
        const bool logicalOn = (activeState == 0) ? (gpioValue == 0) : (gpioValue == 1);
        const bool currentlyOn = (Relay3SP[RELAY_SW_ON].getState() == ISS_ON);
        if (logicalOn != currentlyOn)
        {
            Relay3SP.reset();
            Relay3SP[logicalOn ? RELAY_SW_ON : RELAY_SW_OFF].setState(ISS_ON);
            Relay3SP.setState(logicalOn ? IPS_OK : IPS_IDLE);
            Relay3SP.apply();
        }
    }
}

bool AstrAlimHeater::setPWMDuty(int channel, double percent)
{
    if (channel < 0 || channel > 1)
        return false;

    percent = std::max(0.0, std::min(100.0, percent));

    if (!pwmActors[channel])
        return false;

    try
    {
        pwmActors[channel]->setStepPercent(percent);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

// ==================== Temperature Sensors ====================

std::vector<std::string> AstrAlimHeater::scanDS18B20Devices()
{
    std::vector<std::string> devices;
    DIR* dir = opendir("/sys/bus/w1/devices");
    
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name = entry->d_name;
            if (name.substr(0, 3) == "28-")
            {
                devices.push_back(name);
            }
        }
        closedir(dir);
    }
    
    return devices;
}

bool AstrAlimHeater::readDS18B20Sensors()
{
    for (int ch = 0; ch < 2; ch++)
    {
        INDI::PropertyNumber& tempNP = (ch == 0) ? Heater1TempNP : Heater2TempNP;
        INDI::PropertyText& sensorTP = (ch == 0) ? Heater1SensorTP : Heater2SensorTP;
        
        const char* sensorId = sensorTP[0].getText();
        if (sensorId == nullptr || strlen(sensorId) == 0)
        {
            // Ne pas mettre TEMP_UNAVAILABLE (100°C) si aucun capteur n'est assigné
            // Laisser la valeur à 0 (valeur par défaut) pour éviter d'afficher 100°C
            // L'état IPS_IDLE indique que le capteur n'est pas configuré
            tempNP.setState(IPS_IDLE);
            tempNP.apply();
            continue;
        }
        
        std::string path = "/sys/bus/w1/devices/" + std::string(sensorId) + "/w1_slave";
        std::ifstream file(path);
        
        if (!file.is_open())
        {
            tempNP[0].setValue(TEMP_UNAVAILABLE);
            tempNP.setState(IPS_ALERT);
            tempNP.apply();
            continue;
        }
        
        std::string line1, line2;
        std::getline(file, line1);
        std::getline(file, line2);
        file.close();
        
        // Check CRC
        if (line1.find("YES") == std::string::npos)
        {
            tempNP[0].setValue(TEMP_UNAVAILABLE);
            tempNP.setState(IPS_ALERT);
            tempNP.apply();
            continue;
        }
        
        // Extract temperature
        size_t pos = line2.find("t=");
        if (pos != std::string::npos)
        {
            int tempMilliC = std::stoi(line2.substr(pos + 2));
            double tempC = tempMilliC / 1000.0;
            tempNP[0].setValue(tempC);
            tempNP.setState(IPS_OK);
        }
        else
        {
            tempNP[0].setValue(TEMP_UNAVAILABLE);
            tempNP.setState(IPS_ALERT);
        }
        
        tempNP.apply();
    }
    
    return true;
}

bool AstrAlimHeater::readBME280()
{
    auto& fetcher = AstrAlim::AstraBmeFetcher::getInstance();
    fetcher.setManualHumidity(ManualHumidityNP[0].getValue());
    fetcher.getMeasurement(0, POLL_INTERVAL_MS / 1000.0);

    AmbientNP[AMB_TEMPERATURE].setValue(fetcher.getBmeTemp());
    AmbientNP[AMB_PRESSURE].setValue(fetcher.getBmePressure());
    AmbientNP[AMB_HUMIDITY].setValue(fetcher.getBmeHumidity());

    const double dp = fetcher.getFilteredDewPoint();
    AmbientNP[AMB_DEWPOINT].setValue(dp > DEWPOINT_UNAVAILABLE + 10 ? dp : DEWPOINT_UNAVAILABLE);

    AmbientNP.setState(fetcher.isPresentBme() ? IPS_OK : IPS_ALERT);
    AmbientNP.apply();
    return fetcher.isPresentBme();
}


// ==================== PID Control ====================

double AstrAlimHeater::computePID(int channel, double setpoint, double current)
{
    double error = setpoint - current;
    
    // Proportional
    double pTerm = PIDNP[PID_KP].getValue() * error;
    
    // Integral (with anti-windup)
    pidIntegral[channel] += error;
    pidIntegral[channel] = std::max(-100.0, std::min(100.0, pidIntegral[channel]));
    double iTerm = PIDNP[PID_KI].getValue() * pidIntegral[channel];
    
    // Derivative
    double dTerm = PIDNP[PID_KD].getValue() * (error - pidLastError[channel]);
    pidLastError[channel] = error;
    
    return pTerm + iTerm + dTerm;
}

// ==================== Property Handlers ====================

bool AstrAlimHeater::ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Heater 1 Power (power mode)
        if (Heater1PowerNP.isNameMatch(name))
        {
            if (Heater1ModeSP[MODE_POWER].getState() == ISS_ON)
            {
                Heater1PowerNP.update(values, names, n);
                setPWMDuty(0, Heater1PowerNP[0].getValue());
                Heater1PowerNP.setState(IPS_OK);
                Heater1PowerNP.apply();
                LOGF_INFO("Heater 1 power set to %.0f%%", Heater1PowerNP[0].getValue());
            }
            return true;
        }
        
        // Heater 2 Power (power mode)
        if (Heater2PowerNP.isNameMatch(name))
        {
            if (Heater2ModeSP[MODE_POWER].getState() == ISS_ON)
            {
                Heater2PowerNP.update(values, names, n);
                setPWMDuty(1, Heater2PowerNP[0].getValue());
                Heater2PowerNP.setState(IPS_OK);
                Heater2PowerNP.apply();
                LOGF_INFO("Heater 2 power set to %.0f%%", Heater2PowerNP[0].getValue());
            }
            return true;
        }
        
        // Setpoints
        if (Heater1SetpointNP.isNameMatch(name))
        {
            Heater1SetpointNP.update(values, names, n);
            Heater1SetpointNP.setState(IPS_OK);
            Heater1SetpointNP.apply();
            return true;
        }
        
        if (Heater2SetpointNP.isNameMatch(name))
        {
            Heater2SetpointNP.update(values, names, n);
            Heater2SetpointNP.setState(IPS_OK);
            Heater2SetpointNP.apply();
            return true;
        }
        
        // PID parameters
        if (PIDNP.isNameMatch(name))
        {
            PIDNP.update(values, names, n);
            PIDNP.setState(IPS_OK);
            PIDNP.apply();
            // Reset integrals when PID params change
            pidIntegral[0] = 0;
            pidIntegral[1] = 0;
            LOG_INFO("PID parameters updated");
            return true;
        }
        
        // Dew delta
        if (DewDeltaNP.isNameMatch(name))
        {
            DewDeltaNP.update(values, names, n);
            DewDeltaNP.setState(IPS_OK);
            DewDeltaNP.apply();
            LOGF_INFO("Dew point delta set to %.1f°C", DewDeltaNP[0].getValue());
            return true;
        }
        
        // Manual humidity
        if (ManualHumidityNP.isNameMatch(name))
        {
            ManualHumidityNP.update(values, names, n);
            ManualHumidityNP.setState(IPS_OK);
            ManualHumidityNP.apply();
            LOGF_INFO("Manual humidity set to %.0f%%", ManualHumidityNP[0].getValue());
            return true;
        }
        
    }
    
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool AstrAlimHeater::ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Heater 1 Mode
        if (Heater1ModeSP.isNameMatch(name))
        {
            Heater1ModeSP.update(states, names, n);
            
            if (Heater1ModeSP[MODE_OFF].getState() == ISS_ON)
            {
                setPWMDuty(0, 0);
                Heater1PowerNP[0].setValue(0);
                Heater1ModeSP.setState(IPS_IDLE);
                LOG_INFO("Heater 1 turned OFF");
            }
            else if (Heater1ModeSP[MODE_POWER].getState() == ISS_ON)
            {
                Heater1ModeSP.setState(IPS_OK);
                LOG_INFO("Heater 1 set to POWER mode");
            }
            else if (Heater1ModeSP[MODE_SETPOINT].getState() == ISS_ON)
            {
                pidIntegral[0] = 0;
                pidLastError[0] = 0;
                Heater1ModeSP.setState(IPS_BUSY);
                LOG_INFO("Heater 1 set to SETPOINT mode");
            }
            else if (Heater1ModeSP[MODE_AUTO_DEW].getState() == ISS_ON)
            {
                pidIntegral[0] = 0;
                pidLastError[0] = 0;
                Heater1ModeSP.setState(IPS_BUSY);
                LOG_INFO("Heater 1 set to AUTO DEW mode");
            }
            
            Heater1ModeSP.apply();
            Heater1PowerNP.apply();
            return true;
        }
        
        // Heater 2 Mode
        if (Heater2ModeSP.isNameMatch(name))
        {
            Heater2ModeSP.update(states, names, n);
            
            if (Heater2ModeSP[MODE_OFF].getState() == ISS_ON)
            {
                setPWMDuty(1, 0);
                Heater2PowerNP[0].setValue(0);
                Heater2ModeSP.setState(IPS_IDLE);
                LOG_INFO("Heater 2 turned OFF");
            }
            else if (Heater2ModeSP[MODE_POWER].getState() == ISS_ON)
            {
                Heater2ModeSP.setState(IPS_OK);
                LOG_INFO("Heater 2 set to POWER mode");
            }
            else if (Heater2ModeSP[MODE_SETPOINT].getState() == ISS_ON)
            {
                pidIntegral[1] = 0;
                pidLastError[1] = 0;
                Heater2ModeSP.setState(IPS_BUSY);
                LOG_INFO("Heater 2 set to SETPOINT mode");
            }
            else if (Heater2ModeSP[MODE_AUTO_DEW].getState() == ISS_ON)
            {
                pidIntegral[1] = 0;
                pidLastError[1] = 0;
                Heater2ModeSP.setState(IPS_BUSY);
                LOG_INFO("Heater 2 set to AUTO DEW mode");
            }
            
            Heater2ModeSP.apply();
            Heater2PowerNP.apply();
            return true;
        }
        
        // Heater 1 Sensor Assignment
        if (Heater1SensorAssignSP.isNameMatch(name))
        {
            Heater1SensorAssignSP.update(states, names, n);
            
            if (Heater1SensorAssignSP[SENSOR_ASSIGN_AUTO].getState() == ISS_ON)
            {
                if (autoDetectSensor(0))
                {
                    Heater1SensorAssignSP.setState(IPS_BUSY);
                    LOG_INFO("Heater 1: Auto-detect started");
                }
                else
                {
                    Heater1SensorAssignSP[SENSOR_ASSIGN_AUTO].setState(ISS_OFF);
                    Heater1SensorAssignSP.setState(IPS_ALERT);
                }
            }
            else if (Heater1SensorAssignSP[SENSOR_ASSIGN_TEST].getState() == ISS_ON)
            {
                // Test de réponse pour le capteur actuellement assigné
                if (strlen(Heater1SensorTP[0].getText()) > 0)
                {
                    std::string sensorId = Heater1SensorTP[0].getText();
                    if (testSensorResponse(0, sensorId))
                    {
                        Heater1SensorAssignSP.setState(IPS_BUSY);
                        LOG_INFO("Heater 1: Testing sensor response");
                    }
                    else
                    {
                        Heater1SensorAssignSP[SENSOR_ASSIGN_TEST].setState(ISS_OFF);
                        Heater1SensorAssignSP.setState(IPS_ALERT);
                    }
                }
                else
                {
                    LOG_WARN("Heater 1: No sensor assigned to test");
                    Heater1SensorAssignSP[SENSOR_ASSIGN_TEST].setState(ISS_OFF);
                    Heater1SensorAssignSP.setState(IPS_ALERT);
                }
            }
            else if (Heater1SensorAssignSP[SENSOR_ASSIGN_CLEAR].getState() == ISS_ON)
            {
                // Effacer l'assignation
                Heater1SensorTP[0].setText("");
                ds18b20Path[0] = "";
                Heater1SensorTP.setState(IPS_IDLE);
                Heater1SensorTP.apply();
                Heater1SensorAssignSP[SENSOR_ASSIGN_CLEAR].setState(ISS_OFF);
                Heater1SensorAssignSP.setState(IPS_OK);
                updateSensorStatusList();
                LOG_INFO("Heater 1: Sensor assignment cleared");
            }
            
            Heater1SensorAssignSP.apply();
            return true;
        }
        
        // Heater 2 Sensor Assignment
        if (Heater2SensorAssignSP.isNameMatch(name))
        {
            Heater2SensorAssignSP.update(states, names, n);
            
            if (Heater2SensorAssignSP[SENSOR_ASSIGN_AUTO].getState() == ISS_ON)
            {
                if (autoDetectSensor(1))
                {
                    Heater2SensorAssignSP.setState(IPS_BUSY);
                    LOG_INFO("Heater 2: Auto-detect started");
                }
                else
                {
                    Heater2SensorAssignSP[SENSOR_ASSIGN_AUTO].setState(ISS_OFF);
                    Heater2SensorAssignSP.setState(IPS_ALERT);
                }
            }
            else if (Heater2SensorAssignSP[SENSOR_ASSIGN_TEST].getState() == ISS_ON)
            {
                // Test de réponse pour le capteur actuellement assigné
                if (strlen(Heater2SensorTP[0].getText()) > 0)
                {
                    std::string sensorId = Heater2SensorTP[0].getText();
                    if (testSensorResponse(1, sensorId))
                    {
                        Heater2SensorAssignSP.setState(IPS_BUSY);
                        LOG_INFO("Heater 2: Testing sensor response");
                    }
                    else
                    {
                        Heater2SensorAssignSP[SENSOR_ASSIGN_TEST].setState(ISS_OFF);
                        Heater2SensorAssignSP.setState(IPS_ALERT);
                    }
                }
                else
                {
                    LOG_WARN("Heater 2: No sensor assigned to test");
                    Heater2SensorAssignSP[SENSOR_ASSIGN_TEST].setState(ISS_OFF);
                    Heater2SensorAssignSP.setState(IPS_ALERT);
                }
            }
            else if (Heater2SensorAssignSP[SENSOR_ASSIGN_CLEAR].getState() == ISS_ON)
            {
                // Effacer l'assignation
                Heater2SensorTP[0].setText("");
                ds18b20Path[1] = "";
                Heater2SensorTP.setState(IPS_IDLE);
                Heater2SensorTP.apply();
                Heater2SensorAssignSP[SENSOR_ASSIGN_CLEAR].setState(ISS_OFF);
                Heater2SensorAssignSP.setState(IPS_OK);
                updateSensorStatusList();
                LOG_INFO("Heater 2: Sensor assignment cleared");
            }
            
            Heater2SensorAssignSP.apply();
            return true;
        }
        
        // Available Sensors Selection
        if (AvailableSensorsSP.isNameMatch(name))
        {
            AvailableSensorsSP.update(states, names, n);
            
            // Trouver quel capteur a été sélectionné
            std::string selectedSensorId;
            for (int i = 0; i < 10 && i < static_cast<int>(availableDS18B20.size()); i++)
            {
                if (AvailableSensorsSP[i].getState() == ISS_ON)
                {
                    selectedSensorId = availableDS18B20[i];
                    break;
                }
            }
            
            if (!selectedSensorId.empty())
            {
                AvailableSensorsSP.setState(IPS_OK);
                AvailableSensorsSP.apply();
                LOGF_INFO("Sensor %s selected from list", selectedSensorId.c_str());
            }
            
            return true;
        }
        
        // Sensor Assignment Action
        if (SensorAssignActionSP.isNameMatch(name))
        {
            SensorAssignActionSP.update(states, names, n);
            
            // Trouver quel capteur est sélectionné
            std::string selectedSensorId;
            for (int i = 0; i < 10 && i < static_cast<int>(availableDS18B20.size()); i++)
            {
                if (AvailableSensorsSP[i].getState() == ISS_ON)
                {
                    selectedSensorId = availableDS18B20[i];
                    break;
                }
            }
            
            if (selectedSensorId.empty())
            {
                LOG_WARN("No sensor selected. Please select a sensor from the list first.");
                SensorAssignActionSP[ASSIGN_TO_HEATER1].setState(ISS_OFF);
                SensorAssignActionSP[ASSIGN_TO_HEATER2].setState(ISS_OFF);
                SensorAssignActionSP.setState(IPS_ALERT);
                SensorAssignActionSP.apply();
                return true;
            }
            
            auto getAssignedSensor = [](INDI::PropertyText& sensorTP) -> std::string
            {
                const char* sensorId = sensorTP[0].getText();
                if (sensorId == nullptr || strlen(sensorId) == 0)
                    return "";
                return sensorId;
            };

            auto setHeaterSensor = [this](int heaterChannel, const std::string& sensorId)
            {
                INDI::PropertyText& sensorTP = (heaterChannel == 0) ? Heater1SensorTP : Heater2SensorTP;
                sensorTP[0].setText(sensorId.c_str());

                if (sensorId.empty())
                {
                    ds18b20Path[heaterChannel].clear();
                    sensorTP.setState(IPS_IDLE);
                }
                else
                {
                    ds18b20Path[heaterChannel] = "/sys/bus/w1/devices/" + sensorId + "/w1_slave";
                    sensorTP.setState(IPS_OK);
                }

                sensorTP.apply();
            };

            const std::string heater1SensorId = getAssignedSensor(Heater1SensorTP);
            const std::string heater2SensorId = getAssignedSensor(Heater2SensorTP);

            if (SensorAssignActionSP[ASSIGN_TO_HEATER1].getState() == ISS_ON)
            {
                // Assigner au Heater 1.
                if (heater2SensorId == selectedSensorId &&
                    !heater1SensorId.empty() &&
                    heater1SensorId != selectedSensorId)
                {
                    // Swap H1 <-> H2 when user selects the sensor currently on the other heater.
                    setHeaterSensor(0, selectedSensorId);
                    setHeaterSensor(1, heater1SensorId);
                    LOGF_INFO("Swapped sensors: Heater 1=%s, Heater 2=%s",
                              selectedSensorId.c_str(), heater1SensorId.c_str());
                }
                else
                {
                    // Move or assign to H1.
                    setHeaterSensor(0, selectedSensorId);
                    if (heater2SensorId == selectedSensorId)
                    {
                        setHeaterSensor(1, "");
                    }

                    LOGF_INFO("Sensor %s assigned to Heater 1", selectedSensorId.c_str());
                    autoAssignRemainingSensor(0, selectedSensorId);
                }

                SensorAssignActionSP[ASSIGN_TO_HEATER1].setState(ISS_OFF);
                SensorAssignActionSP.setState(IPS_OK);
                readDS18B20Sensors();
                updateSensorStatusList();
            }
            else if (SensorAssignActionSP[ASSIGN_TO_HEATER2].getState() == ISS_ON)
            {
                // Assigner au Heater 2.
                if (heater1SensorId == selectedSensorId &&
                    !heater2SensorId.empty() &&
                    heater2SensorId != selectedSensorId)
                {
                    // Swap H2 <-> H1 when user selects the sensor currently on the other heater.
                    setHeaterSensor(1, selectedSensorId);
                    setHeaterSensor(0, heater2SensorId);
                    LOGF_INFO("Swapped sensors: Heater 1=%s, Heater 2=%s",
                              heater2SensorId.c_str(), selectedSensorId.c_str());
                }
                else
                {
                    // Move or assign to H2.
                    setHeaterSensor(1, selectedSensorId);
                    if (heater1SensorId == selectedSensorId)
                    {
                        setHeaterSensor(0, "");
                    }

                    LOGF_INFO("Sensor %s assigned to Heater 2", selectedSensorId.c_str());
                    autoAssignRemainingSensor(1, selectedSensorId);
                }

                SensorAssignActionSP[ASSIGN_TO_HEATER2].setState(ISS_OFF);
                SensorAssignActionSP.setState(IPS_OK);
                readDS18B20Sensors();
                updateSensorStatusList();
            }
            
            SensorAssignActionSP.apply();
            return true;
        }

        // Relay active state
        if (ActiveStateSP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Cannot change relay active state while connected");
                return true;
            }

            ActiveStateSP.update(states, names, n);
            activeState = (ActiveStateSP[RELAY_STATE_HIGH].getState() == ISS_ON) ? 1 : 0;
            ActiveStateSP.setState(IPS_OK);
            ActiveStateSP.apply();
            return true;
        }

        // Relay 1
        if (Relay1SP.isNameMatch(name))
        {
            Relay1SP.update(states, names, n);
            const bool turnOn = (Relay1SP[RELAY_SW_ON].getState() == ISS_ON);
            if (!setRelay(0, turnOn))
            {
                Relay1SP.setState(IPS_ALERT);
                Relay1SP.apply();
                return true;
            }

            Relay1SP.setState(turnOn ? IPS_OK : IPS_IDLE);
            Relay1SP.apply();
            return true;
        }

        // Relay 2
        if (Relay2SP.isNameMatch(name))
        {
            Relay2SP.update(states, names, n);
            const bool turnOn = (Relay2SP[RELAY_SW_ON].getState() == ISS_ON);
            if (!setRelay(1, turnOn))
            {
                Relay2SP.setState(IPS_ALERT);
                Relay2SP.apply();
                return true;
            }

            Relay2SP.setState(turnOn ? IPS_OK : IPS_IDLE);
            Relay2SP.apply();
            return true;
        }

        // Relay 3
        if (Relay3SP.isNameMatch(name))
        {
            Relay3SP.update(states, names, n);
            const bool turnOn = (Relay3SP[RELAY_SW_ON].getState() == ISS_ON);
            if (!setRelay(2, turnOn))
            {
                Relay3SP.setState(IPS_ALERT);
                Relay3SP.apply();
                return true;
            }

            Relay3SP.setState(turnOn ? IPS_OK : IPS_IDLE);
            Relay3SP.apply();
            return true;
        }
    }
    
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AstrAlimHeater::ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        if (availableDS18B20.empty())
        {
            availableDS18B20 = scanDS18B20Devices();
        }

        auto getAssignedSensor = [](INDI::PropertyText& sensorTP) -> std::string
        {
            const char* sensorId = sensorTP[0].getText();
            if (sensorId == nullptr || strlen(sensorId) == 0)
                return "";
            return sensorId;
        };

        auto sensorExists = [this](const std::string& sensorId) -> bool
        {
            for (const auto& available : availableDS18B20)
            {
                if (available == sensorId)
                    return true;
            }
            return false;
        };

        // Heater 1 Sensor
        if (Heater1SensorTP.isNameMatch(name))
        {
            const std::string previousH1 = getAssignedSensor(Heater1SensorTP);
            const std::string previousH2 = getAssignedSensor(Heater2SensorTP);

            Heater1SensorTP.update(texts, names, n);

            std::string sensorId = getAssignedSensor(Heater1SensorTP);
            if (sensorId.empty())
            {
                ds18b20Path[0].clear();
                Heater1SensorTP.setState(IPS_IDLE);
                Heater1SensorTP.apply();
                readDS18B20Sensors();
                updateSensorStatusList();
                return true;
            }

            if (!sensorExists(sensorId))
            {
                Heater1SensorTP.setState(IPS_ALERT);
                LOGF_WARN("Heater 1 sensor %s not found in available sensors", sensorId.c_str());
                Heater1SensorTP.apply();
                updateSensorStatusList();
                return true;
            }

            // If sensor is currently assigned to Heater 2, move or swap instead of duplicating.
            if (previousH2 == sensorId)
            {
                if (!previousH1.empty() && previousH1 != sensorId)
                {
                    Heater2SensorTP[0].setText(previousH1.c_str());
                    ds18b20Path[1] = "/sys/bus/w1/devices/" + previousH1 + "/w1_slave";
                    Heater2SensorTP.setState(IPS_OK);
                    Heater2SensorTP.apply();

                    LOGF_INFO("Swapped sensors via text input: Heater 1=%s, Heater 2=%s",
                              sensorId.c_str(), previousH1.c_str());
                }
                else
                {
                    Heater2SensorTP[0].setText("");
                    ds18b20Path[1].clear();
                    Heater2SensorTP.setState(IPS_IDLE);
                    Heater2SensorTP.apply();

                    LOGF_INFO("Moved sensor %s from Heater 2 to Heater 1 via text input",
                              sensorId.c_str());
                }
            }

            ds18b20Path[0] = "/sys/bus/w1/devices/" + sensorId + "/w1_slave";
            Heater1SensorTP.setState(IPS_OK);
            LOGF_INFO("Heater 1 sensor set to %s", sensorId.c_str());
            autoAssignRemainingSensor(0, sensorId);
            Heater1SensorTP.apply();
            readDS18B20Sensors();
            updateSensorStatusList();
            return true;
        }
        
        // Heater 2 Sensor
        if (Heater2SensorTP.isNameMatch(name))
        {
            const std::string previousH1 = getAssignedSensor(Heater1SensorTP);
            const std::string previousH2 = getAssignedSensor(Heater2SensorTP);

            Heater2SensorTP.update(texts, names, n);

            std::string sensorId = getAssignedSensor(Heater2SensorTP);
            if (sensorId.empty())
            {
                ds18b20Path[1].clear();
                Heater2SensorTP.setState(IPS_IDLE);
                Heater2SensorTP.apply();
                readDS18B20Sensors();
                updateSensorStatusList();
                return true;
            }

            if (!sensorExists(sensorId))
            {
                Heater2SensorTP.setState(IPS_ALERT);
                LOGF_WARN("Heater 2 sensor %s not found in available sensors", sensorId.c_str());
                Heater2SensorTP.apply();
                updateSensorStatusList();
                return true;
            }

            // If sensor is currently assigned to Heater 1, move or swap instead of duplicating.
            if (previousH1 == sensorId)
            {
                if (!previousH2.empty() && previousH2 != sensorId)
                {
                    Heater1SensorTP[0].setText(previousH2.c_str());
                    ds18b20Path[0] = "/sys/bus/w1/devices/" + previousH2 + "/w1_slave";
                    Heater1SensorTP.setState(IPS_OK);
                    Heater1SensorTP.apply();

                    LOGF_INFO("Swapped sensors via text input: Heater 1=%s, Heater 2=%s",
                              previousH2.c_str(), sensorId.c_str());
                }
                else
                {
                    Heater1SensorTP[0].setText("");
                    ds18b20Path[0].clear();
                    Heater1SensorTP.setState(IPS_IDLE);
                    Heater1SensorTP.apply();

                    LOGF_INFO("Moved sensor %s from Heater 1 to Heater 2 via text input",
                              sensorId.c_str());
                }
            }

            ds18b20Path[1] = "/sys/bus/w1/devices/" + sensorId + "/w1_slave";
            Heater2SensorTP.setState(IPS_OK);
            LOGF_INFO("Heater 2 sensor set to %s", sensorId.c_str());
            autoAssignRemainingSensor(1, sensorId);
            Heater2SensorTP.apply();
            readDS18B20Sensors();
            updateSensorStatusList();
            return true;
        }
    }
    
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool AstrAlimHeater::saveConfigItems(FILE* fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    
    Heater1ModeSP.save(fp);
    Heater1SetpointNP.save(fp);
    Heater1SensorTP.save(fp);
    Heater2ModeSP.save(fp);
    Heater2SetpointNP.save(fp);
    Heater2SensorTP.save(fp);
    PIDNP.save(fp);
    DewDeltaNP.save(fp);
    ManualHumidityNP.save(fp);
    ActiveStateSP.save(fp);
    Relay1SP.save(fp);
    Relay2SP.save(fp);
    Relay3SP.save(fp);
    Heater1SensorTP.save(fp);
    Heater2SensorTP.save(fp);
    
    return true;
}

// ==================== Sensor Assignment Functions ====================

double AstrAlimHeater::readDS18B20Temperature(const std::string& sensorId)
{
    std::string path = "/sys/bus/w1/devices/" + sensorId + "/w1_slave";
    std::ifstream file(path);
    
    if (!file.is_open())
    {
        return TEMP_UNAVAILABLE;
    }
    
    std::string line1, line2;
    std::getline(file, line1);
    std::getline(file, line2);
    file.close();
    
    // Check CRC
    if (line1.find("YES") == std::string::npos)
    {
        return TEMP_UNAVAILABLE;
    }
    
    // Extract temperature
    size_t pos = line2.find("t=");
    if (pos != std::string::npos)
    {
        int tempMilliC = std::stoi(line2.substr(pos + 2));
        return tempMilliC / 1000.0;
    }
    
    return TEMP_UNAVAILABLE;
}

void AstrAlimHeater::updateSensorStatusList(bool rescanDevices)
{
    // Re-scan périodique ou initial pour détecter les nouveaux capteurs.
    if (rescanDevices || availableDS18B20.empty())
    {
        availableDS18B20 = scanDS18B20Devices();
    }
    
    // Limiter à 10 capteurs maximum
    size_t sensorCount = availableDS18B20.size();
    if (sensorCount > 10) sensorCount = 10;
    
    // Sauvegarder quel capteur est actuellement sélectionné (si la propriété existe déjà)
    std::string selectedSensorId;
    if (AvailableSensorsSP.size() > 0)
    {
        for (size_t i = 0; i < AvailableSensorsSP.size() && i < availableDS18B20.size(); i++)
        {
            if (AvailableSensorsSP[i].getState() == ISS_ON)
            {
                selectedSensorId = availableDS18B20[i];
                break;
            }
        }
    }
    
    // Vérifier si le nombre de capteurs a changé ou si la propriété n'existe pas encore
    bool needRedefine = (AvailableSensorsSP.size() != sensorCount);
    bool propertyExists = (AvailableSensorsSP.size() > 0);
    
    // Si connecté et qu'on a besoin de créer/modifier la propriété
    if (isConnected())
    {
        if (sensorCount > 0 && (needRedefine || !propertyExists))
        {
            // Supprimer l'ancienne propriété si elle existe
            if (propertyExists)
            {
                deleteProperty(AvailableSensorsSP);
            }
            
            // Créer/recréer la propriété avec le bon nombre d'éléments
            AvailableSensorsSP.resize(sensorCount);
            for (size_t i = 0; i < sensorCount; i++)
            {
                char name[32];
                snprintf(name, sizeof(name), "SENSOR_%zu", i);
                AvailableSensorsSP[i].fill(name, "", ISS_OFF);
            }
            AvailableSensorsSP.fill(getDeviceName(), "AVAILABLE_SENSORS", "Available Sensors", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
            defineProperty(AvailableSensorsSP);
            LOGF_INFO("Created AvailableSensorsSP property with %zu sensors", sensorCount);
        }
        else if (sensorCount == 0 && propertyExists)
        {
            // Si aucun capteur, supprimer la propriété
            deleteProperty(AvailableSensorsSP);
            AvailableSensorsSP.resize(0);
        }
    }
    else if (sensorCount > 0 && AvailableSensorsSP.size() == 0)
    {
        // Préparer la propriété même si pas encore connecté (pour initProperties)
        AvailableSensorsSP.resize(sensorCount);
        for (size_t i = 0; i < sensorCount; i++)
        {
            char name[32];
            snprintf(name, sizeof(name), "SENSOR_%zu", i);
            AvailableSensorsSP[i].fill(name, "", ISS_OFF);
        }
        AvailableSensorsSP.fill(getDeviceName(), "AVAILABLE_SENSORS", "Available Sensors", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    }
    
    // Mettre à jour la liste avec les capteurs disponibles (seulement si la propriété existe)
    if (AvailableSensorsSP.size() > 0 && sensorCount > 0)
    {
        for (size_t i = 0; i < sensorCount && i < AvailableSensorsSP.size(); i++)
        {
            const std::string& sensorId = availableDS18B20[i];
            double temp = TEMP_UNAVAILABLE;
            std::string label;
            
            // Vérifier à quel heater ce capteur est assigné
            bool isAssigned = false;
            if (strlen(Heater1SensorTP[0].getText()) > 0 && 
                std::string(Heater1SensorTP[0].getText()) == sensorId)
            {
                label = sensorId + " → H1";
                isAssigned = true;
                // Keep exact visual sync with HEATER1_TEMPERATURE.
                temp = Heater1TempNP[0].getValue();
            }
            else if (strlen(Heater2SensorTP[0].getText()) > 0 && 
                     std::string(Heater2SensorTP[0].getText()) == sensorId)
            {
                label = sensorId + " → H2";
                isAssigned = true;
                // Keep exact visual sync with HEATER2_TEMPERATURE.
                temp = Heater2TempNP[0].getValue();
            }
            else
            {
                label = sensorId;
                temp = readDS18B20Temperature(sensorId);
            }
            
            // Ajouter la température au label
            if (temp < TEMP_UNAVAILABLE - 10)
            {
                char tempStr[32];
                snprintf(tempStr, sizeof(tempStr), " (%.1f°C)", temp);
                label += tempStr;
            }
            else
            {
                label += " (N/A)";
            }
            
            // Déterminer l'état : ON si assigné ou si c'est le capteur sélectionné
            ISState state = ISS_OFF;
            if (isAssigned)
            {
                state = ISS_ON;
            }
            else if (sensorId == selectedSensorId)
            {
                state = ISS_ON;
            }
            
            // Mettre à jour le switch avec fill() pour définir le label
            char name[32];
            snprintf(name, sizeof(name), "SENSOR_%zu", i);
            AvailableSensorsSP[i].fill(name, label.c_str(), state);
        }
        
        AvailableSensorsSP.setState(IPS_OK);
        AvailableSensorsSP.apply();
    }
}

void AstrAlimHeater::validateAssignedSensors()
{
    // Valider Heater 1
    if (strlen(Heater1SensorTP[0].getText()) > 0)
    {
        std::string sensorId = Heater1SensorTP[0].getText();
        bool found = false;
        for (const auto& available : availableDS18B20)
        {
            if (available == sensorId)
            {
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            Heater1SensorTP.setState(IPS_ALERT);
            LOGF_WARN("Heater 1 assigned sensor %s not found!", sensorId.c_str());
        }
        else
        {
            double temp = readDS18B20Temperature(sensorId);
            if (temp < TEMP_UNAVAILABLE - 10)
            {
                Heater1SensorTP.setState(IPS_OK);
            }
            else
            {
                Heater1SensorTP.setState(IPS_BUSY);
            }
        }
        Heater1SensorTP.apply();
    }
    
    // Valider Heater 2
    if (strlen(Heater2SensorTP[0].getText()) > 0)
    {
        std::string sensorId = Heater2SensorTP[0].getText();
        bool found = false;
        for (const auto& available : availableDS18B20)
        {
            if (available == sensorId)
            {
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            Heater2SensorTP.setState(IPS_ALERT);
            LOGF_WARN("Heater 2 assigned sensor %s not found!", sensorId.c_str());
        }
        else
        {
            double temp = readDS18B20Temperature(sensorId);
            if (temp < TEMP_UNAVAILABLE - 10)
            {
                Heater2SensorTP.setState(IPS_OK);
            }
            else
            {
                Heater2SensorTP.setState(IPS_BUSY);
            }
        }
        Heater2SensorTP.apply();
    }
}

void AstrAlimHeater::autoAssignRemainingSensor(int assignedHeaterChannel, const std::string& assignedSensorId)
{
    if (assignedHeaterChannel < 0 || assignedHeaterChannel > 1)
        return;

    if (availableDS18B20.empty())
    {
        availableDS18B20 = scanDS18B20Devices();
    }

    int otherHeater = (assignedHeaterChannel == 0) ? 1 : 0;
    INDI::PropertyText& otherSensorTP = (otherHeater == 0) ? Heater1SensorTP : Heater2SensorTP;

    // Never overwrite an explicit user assignment.
    if (strlen(otherSensorTP[0].getText()) > 0)
        return;

    std::string candidateSensorId;
    for (const auto& sensorId : availableDS18B20)
    {
        if (sensorId == assignedSensorId)
            continue;

        bool usedByH1 = (strlen(Heater1SensorTP[0].getText()) > 0 &&
                         std::string(Heater1SensorTP[0].getText()) == sensorId);
        bool usedByH2 = (strlen(Heater2SensorTP[0].getText()) > 0 &&
                         std::string(Heater2SensorTP[0].getText()) == sensorId);

        if (usedByH1 || usedByH2)
            continue;

        // Ambiguous remaining choice: do not auto-assign.
        if (!candidateSensorId.empty())
            return;

        candidateSensorId = sensorId;
    }

    if (candidateSensorId.empty())
        return;

    otherSensorTP[0].setText(candidateSensorId.c_str());
    ds18b20Path[otherHeater] = "/sys/bus/w1/devices/" + candidateSensorId + "/w1_slave";
    otherSensorTP.setState(IPS_OK);
    otherSensorTP.apply();

    LOGF_INFO("Auto-assigned remaining sensor %s to Heater %d",
              candidateSensorId.c_str(), otherHeater + 1);
}

bool AstrAlimHeater::autoDetectSensor(int heaterChannel)
{
    if (heaterChannel < 0 || heaterChannel > 1)
        return false;
    
    INDI::PropertySwitch& modeSP = (heaterChannel == 0) ? Heater1ModeSP : Heater2ModeSP;
    INDI::PropertyNumber& powerNP = (heaterChannel == 0) ? Heater1PowerNP : Heater2PowerNP;
    
    // Vérifier que le heater n'est pas déjà en mode AUTO_DEW ou POWER avec puissance
    if (modeSP[MODE_AUTO_DEW].getState() == ISS_ON || 
        (modeSP[MODE_POWER].getState() == ISS_ON && powerNP[0].getValue() > 0))
    {
        LOG_ERROR("Cannot start auto-detect: heater must be in OFF mode or POWER with 0% power");
        return false;
    }
    
    // Activer le mode POWER temporairement
    modeSP[MODE_OFF].setState(ISS_OFF);
    modeSP[MODE_POWER].setState(ISS_ON);
    modeSP[MODE_SETPOINT].setState(ISS_OFF);
    modeSP[MODE_AUTO_DEW].setState(ISS_OFF);
    modeSP.setState(IPS_BUSY);
    modeSP.apply();
    
    // Lire les températures de départ de tous les capteurs
    for (size_t i = 0; i < availableDS18B20.size() && i < 10; i++)
    {
        sensorAssignState[heaterChannel].testStartTemp[i] = 
            readDS18B20Temperature(availableDS18B20[i]);
    }
    
    // Activer le heater à 50% pour le test
    sensorAssignState[heaterChannel].testPower = 50;
    setPWMDuty(heaterChannel, sensorAssignState[heaterChannel].testPower);
    powerNP[0].setValue(sensorAssignState[heaterChannel].testPower);
    powerNP.setState(IPS_BUSY);
    powerNP.apply();
    
    // Démarrer le test
    sensorAssignState[heaterChannel].autoDetectActive = true;
    sensorAssignState[heaterChannel].testStartTime = std::chrono::steady_clock::now();
    
    LOGF_INFO("Heater %d: Auto-detect started (Power: %d%%)", 
              heaterChannel + 1, sensorAssignState[heaterChannel].testPower);
    
    return true;
}

void AstrAlimHeater::handleAutoDetect(int heaterChannel)
{
    if (heaterChannel < 0 || heaterChannel > 1)
        return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - sensorAssignState[heaterChannel].testStartTime).count();
    
    INDI::PropertySwitch& modeSP = (heaterChannel == 0) ? Heater1ModeSP : Heater2ModeSP;
    INDI::PropertyNumber& powerNP = (heaterChannel == 0) ? Heater1PowerNP : Heater2PowerNP;
    INDI::PropertyText& sensorTP = (heaterChannel == 0) ? Heater1SensorTP : Heater2SensorTP;
    INDI::PropertySwitch& assignSP = (heaterChannel == 0) ? Heater1SensorAssignSP : Heater2SensorAssignSP;
    
    // Durée du test : 2 minutes
    const int TEST_DURATION_SEC = 120;
    
    if (elapsed >= TEST_DURATION_SEC)
    {
        // Test terminé, analyser les résultats
        double maxTempIncrease = 0;
        int bestSensorIndex = -1;
        
        for (size_t i = 0; i < availableDS18B20.size() && i < 10; i++)
        {
            double currentTemp = readDS18B20Temperature(availableDS18B20[i]);
            double startTemp = sensorAssignState[heaterChannel].testStartTemp[i];
            
            if (startTemp < TEMP_UNAVAILABLE - 10 && currentTemp < TEMP_UNAVAILABLE - 10)
            {
                double tempIncrease = currentTemp - startTemp;
                if (tempIncrease > maxTempIncrease)
                {
                    maxTempIncrease = tempIncrease;
                    bestSensorIndex = i;
                }
            }
        }
        
        // Arrêter le heater
        setPWMDuty(heaterChannel, 0);
        powerNP[0].setValue(0);
        modeSP[MODE_OFF].setState(ISS_ON);
        modeSP[MODE_POWER].setState(ISS_OFF);
        modeSP[MODE_SETPOINT].setState(ISS_OFF);
        modeSP[MODE_AUTO_DEW].setState(ISS_OFF);
        modeSP.setState(IPS_IDLE);
        modeSP.apply();
        powerNP.apply();
        
        // Assigner le meilleur capteur trouvé
        if (bestSensorIndex >= 0 && maxTempIncrease >= 0.5)  // Au moins 0.5°C d'augmentation
        {
            std::string bestSensorId = availableDS18B20[bestSensorIndex];
            sensorTP[0].setText(bestSensorId.c_str());
            ds18b20Path[heaterChannel] = "/sys/bus/w1/devices/" + bestSensorId + "/w1_slave";
            sensorTP.setState(IPS_OK);
            sensorTP.apply();
            
            LOGF_INFO("Heater %d: Auto-detect completed - Assigned sensor %s (Temp increase: %.2f°C)", 
                      heaterChannel + 1, bestSensorId.c_str(), maxTempIncrease);
        }
        else
        {
            LOG_WARN("Auto-detect completed but no suitable sensor found (increase < 0.5°C)");
        }
        
        // Réinitialiser l'état
        sensorAssignState[heaterChannel].autoDetectActive = false;
        assignSP[0].setState(ISS_OFF);
        assignSP.setState(IPS_IDLE);
        assignSP.apply();
        
        // Mettre à jour la liste des capteurs
        updateSensorStatusList();
    }
}

bool AstrAlimHeater::testSensorResponse(int heaterChannel, const std::string& sensorId)
{
    // Cette fonction peut être utilisée pour tester un capteur spécifique
    // Pour l'instant, on utilise la même logique que autoDetectSensor
    // mais avec un capteur spécifique
    
    double startTemp = readDS18B20Temperature(sensorId);
    if (startTemp >= TEMP_UNAVAILABLE - 10)
    {
        LOGF_ERROR("Cannot test sensor %s: invalid temperature reading", sensorId.c_str());
        return false;
    }
    
    LOGF_INFO("Testing sensor %s response for Heater %d", sensorId.c_str(), heaterChannel + 1);
    
    // Le test sera géré par handleAutoDetect avec un capteur spécifique
    // Pour simplifier, on utilise autoDetectSensor qui teste tous les capteurs
    return autoDetectSensor(heaterChannel);
}

void AstrAlimHeater::readINA219()
{
    enum class InaTarget { H1, H2, DC1, DC2, DC3 };

    struct InaEntry
    {
        int address;
        std::unique_ptr<AstrAlim::AstraIna>* sensor;
        InaTarget target;
        bool isRelay;
    };

    std::array<InaEntry, 5> entries = {{
        {INA_ADDR_H1, &inaSensors[0], InaTarget::H1, false},
        {INA_ADDR_H2, &inaSensors[1], InaTarget::H2, false},
        {INA_RELAY_DC1_ADDR, &relayInaSensors[0], InaTarget::DC1, true},
        {INA_RELAY_DC2_ADDR, &relayInaSensors[1], InaTarget::DC2, true},
        {INA_RELAY_DC3_ADDR, &relayInaSensors[2], InaTarget::DC3, true},
    }};

    double totalCurrent = 0.0;
    double totalPower = 0.0;

    for (auto& entry : entries)
    {
        double voltageV = 0.0;
        double currentA = 0.0;
        double powerW = 0.0;
        bool valid = false;

        try
        {
            if (!(*entry.sensor))
            {
                *entry.sensor = std::make_unique<AstrAlim::AstraIna>(0.01, 6.0, 1, entry.address, "", true);
            }

            valid = (*entry.sensor)->getPingOK() && ((*entry.sensor)->intPeriodS() > 0.0);
            if (valid)
            {
                voltageV = std::max(0.0, (*entry.sensor)->voltageV());
                currentA = std::max(0.0, std::fabs((*entry.sensor)->currentA()));
                powerW = std::max(0.0, (*entry.sensor)->powerW());

                if (!entry.isRelay)
                {
                    const int heaterIdx = (entry.target == InaTarget::H1) ? 0 : 1;
                    inaDisplayVoltage[heaterIdx] = voltageV;
                    inaDisplayCurrent[heaterIdx] = currentA;
                    inaDisplayPower[heaterIdx] = powerW;
                }
                else
                {
                    totalCurrent += currentA;
                    totalPower += powerW;
                }
            }
        }
        catch (const std::exception&)
        {
            (*entry.sensor).reset();
            valid = false;
        }

        switch (entry.target)
        {
            case InaTarget::H1:
                HeaterPower1NP[HEATER_PWR_VOLTAGE].setValue(voltageV);
                HeaterPower1NP[HEATER_PWR_CURRENT].setValue(currentA);
                HeaterPower1NP[HEATER_PWR_POWER].setValue(powerW);
                HeaterPower1NP.setState(valid ? IPS_OK : IPS_IDLE);
                HeaterPower1NP.apply();
                break;
            case InaTarget::H2:
                HeaterPower2NP[HEATER_PWR_VOLTAGE].setValue(voltageV);
                HeaterPower2NP[HEATER_PWR_CURRENT].setValue(currentA);
                HeaterPower2NP[HEATER_PWR_POWER].setValue(powerW);
                HeaterPower2NP.setState(valid ? IPS_OK : IPS_IDLE);
                HeaterPower2NP.apply();
                break;
            case InaTarget::DC1:
                RelayPowerDC1NP[RELAY_PWR_VOLTAGE].setValue(voltageV);
                RelayPowerDC1NP[RELAY_PWR_CURRENT].setValue(currentA);
                RelayPowerDC1NP[RELAY_PWR_POWER].setValue(powerW);
                RelayPowerDC1NP.setState(valid ? IPS_OK : IPS_IDLE);
                RelayPowerDC1NP.apply();
                break;
            case InaTarget::DC2:
                RelayPowerDC2NP[RELAY_PWR_VOLTAGE].setValue(voltageV);
                RelayPowerDC2NP[RELAY_PWR_CURRENT].setValue(currentA);
                RelayPowerDC2NP[RELAY_PWR_POWER].setValue(powerW);
                RelayPowerDC2NP.setState(valid ? IPS_OK : IPS_IDLE);
                RelayPowerDC2NP.apply();
                break;
            case InaTarget::DC3:
                RelayPowerDC3NP[RELAY_PWR_VOLTAGE].setValue(voltageV);
                RelayPowerDC3NP[RELAY_PWR_CURRENT].setValue(currentA);
                RelayPowerDC3NP[RELAY_PWR_POWER].setValue(powerW);
                RelayPowerDC3NP.setState(valid ? IPS_OK : IPS_IDLE);
                RelayPowerDC3NP.apply();
                break;
        }
    }

    RelayTotalPowerNP[0].setValue(totalCurrent);
    relayTotalEnergyWh += totalPower * (static_cast<double>(POLL_INTERVAL_MS) / 3600000.0);
    RelayTotalPowerNP[1].setValue(relayTotalEnergyWh);
    RelayTotalPowerNP.setState((totalCurrent > 0.0 || totalPower > 0.0) ? IPS_OK : IPS_IDLE);
    RelayTotalPowerNP.apply();
}

bool AstrAlimHeater::resetINAChannel(int channel)
{
    if (channel < 0 || channel > 1)
        return false;

    const int address = (channel == 0) ? INA_ADDR_H1 : INA_ADDR_H2;

    try
    {
        inaSensors[channel].reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        inaSensors[channel] = std::make_unique<AstrAlim::AstraIna>(0.01, 6.0, 1, address, "", true);
        return true;
    }
    catch (const std::exception&)
    {
        inaSensors[channel].reset();
        return false;
    }
}

void AstrAlimHeater::resetINADisplayState()
{
    auto now = std::chrono::steady_clock::now();
    for (int ch = 0; ch < 2; ch++)
    {
        inaDisplayVoltage[ch] = 0.0;
        inaDisplayCurrent[ch] = 0.0;
        inaDisplayPower[ch] = 0.0;
        inaHasSample[ch] = false;
        inaInvalidCount[ch] = 0;
        inaZeroWhileActiveCount[ch] = 0;
        inaLastErrorTag[ch].clear();
        inaErrorLogCount[ch] = 0;
        inaNoResponseActive[ch] = false;
        inaNoResponseSince[ch] = now;
        inaLastResetAttempt[ch] = now - std::chrono::milliseconds(INA_RESET_COOLDOWN_MS);
    }
}

void AstrAlimHeater::applyINAChannelSample(int channel, bool validSample, bool currentValid, double sampleVoltage, double sampleCurrent, bool heaterActive)
{
    if (channel < 0 || channel > 1)
        return;

    if (!validSample)
    {
        inaInvalidCount[channel]++;

        if (heaterActive && !inaHasSample[channel] && inaInvalidCount[channel] >= INA_STARTUP_FALLBACK_CYCLES)
        {
            inaDisplayVoltage[channel] = std::max(inaDisplayVoltage[channel], 12.0);
            inaDisplayCurrent[channel] = std::max(inaDisplayCurrent[channel], 0.0);
            inaHasSample[channel] = true;
        }

        if (!inaHasSample[channel])
            return;

        if (!heaterActive && inaInvalidCount[channel] >= 2)
        {
            inaDisplayVoltage[channel] *= 0.60;
            inaDisplayCurrent[channel] *= 0.60;
            if (inaDisplayVoltage[channel] < 0.05)
                inaDisplayVoltage[channel] = 0.0;
            if (inaDisplayCurrent[channel] < 0.01)
                inaDisplayCurrent[channel] = 0.0;
        }

        if (inaInvalidCount[channel] >= INA_INVALID_RESET_CYCLES)
        {
            inaDisplayVoltage[channel] = 0.0;
            inaDisplayCurrent[channel] = 0.0;
            inaHasSample[channel] = false;
        }
        return;
    }

    inaInvalidCount[channel] = 0;
    sampleVoltage = std::max(0.0, sampleVoltage);
    sampleCurrent = std::max(0.0, sampleCurrent);

    const double currentForNearZero = currentValid ? sampleCurrent : inaDisplayCurrent[channel];
    const bool nearZeroSample = (sampleVoltage < 0.20 && currentForNearZero < 0.02);
    if (heaterActive && nearZeroSample && inaHasSample[channel] && inaDisplayCurrent[channel] > 0.05)
    {
        inaZeroWhileActiveCount[channel]++;
        if (inaZeroWhileActiveCount[channel] <= INA_ZERO_GLITCH_HOLD_CYCLES)
            return;
    }
    else
    {
        inaZeroWhileActiveCount[channel] = 0;
    }

    if (!inaHasSample[channel])
    {
        inaDisplayVoltage[channel] = sampleVoltage;
        inaDisplayCurrent[channel] = currentValid ? sampleCurrent : inaDisplayCurrent[channel];
        inaHasSample[channel] = true;
        return;
    }

    const double alpha = heaterActive ? INA_FILTER_ALPHA_ACTIVE : INA_FILTER_ALPHA_IDLE;
    inaDisplayVoltage[channel] = (alpha * sampleVoltage) + ((1.0 - alpha) * inaDisplayVoltage[channel]);
    if (currentValid)
    {
        inaDisplayCurrent[channel] = (alpha * sampleCurrent) + ((1.0 - alpha) * inaDisplayCurrent[channel]);
    }
    else if (!heaterActive)
    {
        inaDisplayCurrent[channel] *= (1.0 - alpha);
    }

    if (!heaterActive && nearZeroSample)
    {
        if (inaDisplayVoltage[channel] < 0.05)
            inaDisplayVoltage[channel] = 0.0;
        if (inaDisplayCurrent[channel] < 0.01)
            inaDisplayCurrent[channel] = 0.0;
    }
}
