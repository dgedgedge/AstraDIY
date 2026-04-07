/*******************************************************************************
 * AstrAlim Heater Driver - Dew Heater Controller
 * Copyright (c) 2024 AstrAlim Project
 ******************************************************************************/

#include "astralim_heater.h"
#include "astralim_gpio.h"
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
    
    // Initialiser les filtres
    filteredDewPoint = DEWPOINT_UNAVAILABLE;
    filteredTemp = 0.0;
    filteredHumidity = 0.0;
    dewPointHistory.clear();
    tempHistory.clear();
    humidityHistory.clear();
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
    AmbientNP.fill(getDeviceName(), "AMBIENT_SENSOR", "Ambient", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Manual humidity (for BMP280)
    ManualHumidityNP[0].fill("MANUAL_HUMIDITY", "Humidity (%)", "%.0f", 0, 100, 5, 50);
    ManualHumidityNP.fill(getDeviceName(), "MANUAL_HUMIDITY", "Manual Humidity", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
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
    Heater1ModeSP[MODE_MANUAL].fill("HEATER1_MANUAL", "Manual", ISS_OFF);
    Heater1ModeSP[MODE_AUTO].fill("HEATER1_AUTO", "Auto (Dew)", ISS_OFF);
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
    Heater2ModeSP[MODE_MANUAL].fill("HEATER2_MANUAL", "Manual", ISS_OFF);
    Heater2ModeSP[MODE_AUTO].fill("HEATER2_AUTO", "Auto (Dew)", ISS_OFF);
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
    DewDeltaNP.fill(getDeviceName(), "DEW_DELTA", "Dew Point Delta", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // ===== Power Monitoring =====
    PowerMonitorNP[PWR_VOLTAGE1].fill("VOLTAGE1", "Heater 1 (V)", "%.2f", 0, 15, 0, 0);
    PowerMonitorNP[PWR_CURRENT1].fill("CURRENT1", "Heater 1 (A)", "%.2f", 0, 6, 0, 0);
    PowerMonitorNP[PWR_VOLTAGE2].fill("VOLTAGE2", "Heater 2 (V)", "%.2f", 0, 15, 0, 0);
    PowerMonitorNP[PWR_CURRENT2].fill("CURRENT2", "Heater 2 (A)", "%.2f", 0, 6, 0, 0);
    PowerMonitorNP.fill(getDeviceName(), "POWER_MONITOR", "Power Monitor", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    addDebugControl();
    setDefaultPollingPeriod(POLL_INTERVAL_MS);
    
    return true;
}

bool AstrAlimHeater::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    
    if (isConnected())
    {
        defineProperty(AmbientNP);
        defineProperty(ManualHumidityNP);
        defineProperty(Heater1TempNP);
        defineProperty(Heater1PowerNP);
        defineProperty(Heater1SetpointNP);
        defineProperty(Heater1ModeSP);
        defineProperty(Heater1SensorTP);
        defineProperty(Heater1SensorAssignSP);
        defineProperty(Heater2TempNP);
        defineProperty(Heater2PowerNP);
        defineProperty(Heater2SetpointNP);
        defineProperty(Heater2ModeSP);
        defineProperty(Heater2SensorTP);
        defineProperty(Heater2SensorAssignSP);
        // AvailableSensorsSP sera défini dynamiquement après le scan
        if (AvailableSensorsSP.count() > 0)
        {
            defineProperty(AvailableSensorsSP);
        }
        defineProperty(SensorAssignActionSP);
        defineProperty(PIDNP);
        defineProperty(DewDeltaNP);
        defineProperty(PowerMonitorNP);
    }
    else
    {
        deleteProperty(AmbientNP);
        deleteProperty(ManualHumidityNP);
        deleteProperty(Heater1TempNP);
        deleteProperty(Heater1PowerNP);
        deleteProperty(Heater1SetpointNP);
        deleteProperty(Heater1ModeSP);
        deleteProperty(Heater1SensorTP);
        deleteProperty(Heater1SensorAssignSP);
        deleteProperty(Heater2TempNP);
        deleteProperty(Heater2PowerNP);
        deleteProperty(Heater2SetpointNP);
        deleteProperty(Heater2ModeSP);
        deleteProperty(Heater2SensorTP);
        deleteProperty(Heater2SensorAssignSP);
        if (AvailableSensorsSP.count() > 0)
        {
            deleteProperty(AvailableSensorsSP);
        }
        deleteProperty(SensorAssignActionSP);
        deleteProperty(PIDNP);
        deleteProperty(DewDeltaNP);
        deleteProperty(PowerMonitorNP);
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
    Heater1ModeSP[MODE_MANUAL].setState(ISS_OFF);
    Heater1ModeSP[MODE_AUTO].setState(ISS_OFF);
    Heater1ModeSP.setState(IPS_IDLE);
    Heater1ModeSP.apply();
    
    Heater2ModeSP[MODE_OFF].setState(ISS_ON);
    Heater2ModeSP[MODE_MANUAL].setState(ISS_OFF);
    Heater2ModeSP[MODE_AUTO].setState(ISS_OFF);
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
        else if (modeSP[MODE_MANUAL].getState() == ISS_ON)
        {
            // Manual mode - use power directly
            if (pidRunning[ch])
            {
                pidRunning[ch] = false;
                if (pidThread[ch].joinable())
                    pidThread[ch].join();
            }
            setPWMDuty(ch, powerNP[0].getValue());
            powerNP.setState(IPS_OK);
        }
        else if (modeSP[MODE_AUTO].getState() == ISS_ON)
        {
            // Auto mode - PID control based on dew point
            // Utiliser le point de rosée filtré pour éviter les variations
            double dewPoint = filteredDewPoint;
            
            // Si le filtre n'est pas encore initialisé, utiliser la valeur brute
            if (dewPoint <= DEWPOINT_UNAVAILABLE + 10)
            {
                dewPoint = AmbientNP[AMB_DEWPOINT].getValue();
            }
            
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

std::array<bool, 10> AstrAlimHeater::buildStepPattern(double percent) const
{
    std::array<bool, 10> pattern {};
    percent = std::max(0.0, std::min(100.0, percent));

    const int stepCount = STEP_PWM_STEP_COUNT;
    int highStepCount = static_cast<int>(std::lround(stepCount * percent / 100.0));
    highStepCount = std::max(0, std::min(highStepCount, stepCount));

    if (highStepCount == 0)
        return pattern;

    if (highStepCount >= stepCount)
    {
        pattern.fill(true);
        return pattern;
    }

    std::set<int> distributedSteps;
    for (int i = 0; i < highStepCount; i++)
    {
        int step = static_cast<int>(std::lround((static_cast<double>(i) * stepCount) / highStepCount)) % stepCount;
        distributedSteps.insert(step);
    }

    for (int candidate = 0; candidate < stepCount && static_cast<int>(distributedSteps.size()) < highStepCount; candidate++)
    {
        distributedSteps.insert(candidate);
    }

    for (int step : distributedSteps)
    {
        pattern[step] = true;
    }

    return pattern;
}

bool AstrAlimHeater::initPWM()
{
    if (!pwmGpio->openChip(AstrAlim::RPI5_GPIO_CHIP))
    {
        LOGF_ERROR("Failed to open GPIO chip %s for step PWM", AstrAlim::RPI5_GPIO_CHIP);
        return false;
    }

    const int gpioPins[2] = { STEP_PWM_GPIO_H1, STEP_PWM_GPIO_H2 };
    for (int ch = 0; ch < 2; ch++)
    {
        if (pwmGpio->isLineUsed(gpioPins[ch]))
        {
            std::string consumer = pwmGpio->getLineConsumer(gpioPins[ch]);
            if (!consumer.empty())
            {
                LOGF_ERROR("GPIO %d is already used by '%s'. Stop HMI/other process and retry.", gpioPins[ch], consumer.c_str());
            }
            else
            {
                LOGF_ERROR("GPIO %d is already used by another process. Stop HMI/other process and retry.", gpioPins[ch]);
            }
            pwmGpio->closeChip();
            return false;
        }
    }

    for (int ch = 0; ch < 2; ch++)
    {
        char consumer[64];
        snprintf(consumer, sizeof(consumer), "heater%d@astralim_heater", ch + 1);
        if (!pwmGpio->requestOutput(gpioPins[ch], consumer, 0))
        {
            LOGF_ERROR("Failed to request GPIO %d for step PWM", gpioPins[ch]);
            pwmGpio->closeChip();
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(pwmStepMutex);
        pwmDutyPercent[0] = 0.0;
        pwmDutyPercent[1] = 0.0;
        pwmStepPattern[0] = buildStepPattern(0.0);
        pwmStepPattern[1] = buildStepPattern(0.0);
        pwmStepIndex = 0;
    }

    try
    {
        pwmStepRunning = true;
        pwmStepThread = std::thread(&AstrAlimHeater::runStepPwmLoop, this);
    }
    catch (const std::exception& e)
    {
        pwmStepRunning = false;
        LOGF_ERROR("Failed to start step PWM thread: %s", e.what());
        pwmGpio->closeChip();
        return false;
    }
    LOGF_INFO("Step PWM started: tick=%dms steps=%d (Heater1 GPIO%d, Heater2 GPIO%d)",
              STEP_PWM_TICK_MS, STEP_PWM_STEP_COUNT, STEP_PWM_GPIO_H1, STEP_PWM_GPIO_H2);

    return true;
}

void AstrAlimHeater::closePWM()
{
    pwmStepRunning = false;
    pwmStepCv.notify_all();

    if (pwmStepThread.joinable())
    {
        pwmStepThread.join();
    }

    if (pwmGpio && pwmGpio->isOpen())
    {
        pwmGpio->setValue(STEP_PWM_GPIO_H1, 0);
        pwmGpio->setValue(STEP_PWM_GPIO_H2, 0);
        pwmGpio->closeChip();
    }

    std::lock_guard<std::mutex> lock(pwmStepMutex);
    pwmDutyPercent[0] = 0.0;
    pwmDutyPercent[1] = 0.0;
    pwmStepPattern[0] = buildStepPattern(0.0);
    pwmStepPattern[1] = buildStepPattern(0.0);
    pwmStepIndex = 0;
}

bool AstrAlimHeater::setPWMDuty(int channel, double percent)
{
    if (channel < 0 || channel > 1)
        return false;

    percent = std::max(0.0, std::min(100.0, percent));

    {
        std::lock_guard<std::mutex> lock(pwmStepMutex);
        pwmDutyPercent[channel] = percent;
        pwmStepPattern[channel] = buildStepPattern(percent);
    }
    pwmStepCv.notify_one();

    return true;
}

void AstrAlimHeater::runStepPwmLoop()
{
    while (pwmStepRunning)
    {
        int valueH1 = 0;
        int valueH2 = 0;
        {
            std::lock_guard<std::mutex> lock(pwmStepMutex);
            valueH1 = pwmStepPattern[0][pwmStepIndex] ? 1 : 0;
            valueH2 = pwmStepPattern[1][pwmStepIndex] ? 1 : 0;
            pwmStepIndex = (pwmStepIndex + 1) % STEP_PWM_STEP_COUNT;
        }

        if (pwmGpio && pwmGpio->isOpen())
        {
            pwmGpio->setValue(STEP_PWM_GPIO_H1, valueH1);
            pwmGpio->setValue(STEP_PWM_GPIO_H2, valueH2);
        }

        std::unique_lock<std::mutex> lock(pwmStepMutex);
        pwmStepCv.wait_for(lock, std::chrono::milliseconds(STEP_PWM_TICK_MS),
                           [this]() { return !pwmStepRunning.load(); });
    }

    if (pwmGpio && pwmGpio->isOpen())
    {
        pwmGpio->setValue(STEP_PWM_GPIO_H1, 0);
        pwmGpio->setValue(STEP_PWM_GPIO_H2, 0);
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
    // Try to read BME280/BMP280 via i2cget or python helper
    // The sensor is typically at address 0x76 or 0x77 on bus 1
    
    // Use a simple approach: read from a helper script or direct i2c
    // For now, we'll use a Python one-liner approach
    
    std::string result = execCommand(
        "python3 -c \""
        "try:\n"
        "    from lib.bme280_lib import readBME280All\n"
        "    t,p,h = readBME280All()\n"
        "    print(f'{t},{p},{h}')\n"
        "except:\n"
        "    try:\n"
        "        import smbus2\n"
        "        import bme280\n"
        "        bus = smbus2.SMBus(1)\n"
        "        cal = bme280.load_calibration_params(bus, 0x76)\n"
        "        data = bme280.sample(bus, 0x76, cal)\n"
        "        print(f'{data.temperature},{data.pressure},{data.humidity}')\n"
        "    except:\n"
        "        print('error')\n"
        "\" 2>/dev/null"
    );
    
    if (result.empty() || result.find("error") != std::string::npos)
    {
        // Try alternate approach with direct file read if available
        AmbientNP.setState(IPS_ALERT);
        AmbientNP.apply();
        return false;
    }
    
    // Parse result: temp,pressure,humidity
    std::istringstream iss(result);
    std::string token;
    std::vector<double> values;
    
    while (std::getline(iss, token, ','))
    {
        try
        {
            values.push_back(std::stod(token));
        }
        catch (...)
        {
            values.push_back(0);
        }
    }
    
    if (values.size() >= 3)
    {
        double temp = values[0];
        double pressure = values[1];
        double humidity = values[2];
        
        // If humidity is 0 (BMP280), use manual humidity
        if (humidity == 0)
        {
            humidity = ManualHumidityNP[0].getValue();
        }
        
        // Filtrer la température et l'humidité avant de calculer le point de rosée
        filteredTemp = filterValue(temp, tempHistory, TEMP_HUMIDITY_MIN_CHANGE);
        filteredHumidity = filterValue(humidity, humidityHistory, TEMP_HUMIDITY_MIN_CHANGE);
        
        AmbientNP[AMB_TEMPERATURE].setValue(temp);
        AmbientNP[AMB_PRESSURE].setValue(pressure);
        AmbientNP[AMB_HUMIDITY].setValue(humidity);
        
        // Calculate dew point using filtered values for stability
        if (humidity > 0)
        {
            // Utiliser les valeurs filtrées pour calculer le point de rosée
            double dewPoint = calculateDewPoint(filteredTemp, filteredHumidity);
            
            // Filtrer également le point de rosée calculé avec une moyenne mobile
            filteredDewPoint = filterDewPoint(dewPoint);
            
            AmbientNP[AMB_DEWPOINT].setValue(filteredDewPoint);
        }
        else
        {
            // Réinitialiser les filtres si pas d'humidité
            dewPointHistory.clear();
            tempHistory.clear();
            humidityHistory.clear();
            filteredDewPoint = DEWPOINT_UNAVAILABLE;
            AmbientNP[AMB_DEWPOINT].setValue(DEWPOINT_UNAVAILABLE);
        }
        
        AmbientNP.setState(IPS_OK);
    }
    else
    {
        AmbientNP.setState(IPS_ALERT);
    }
    
    AmbientNP.apply();
    return true;
}

double AstrAlimHeater::calculateDewPoint(double temp, double humidity)
{
    // Magnus formula
    // ref: https://fr.wikipedia.org/wiki/Point_de_ros%C3%A9e
    const double a = 17.27;
    const double b = 237.7;
    
    double alpha = ((a * temp) / (b + temp)) + std::log(humidity / 100.0);
    double dewPoint = (b * alpha) / (a - alpha);
    
    return dewPoint;
}

double AstrAlimHeater::filterDewPoint(double newDewPoint)
{
    // Si l'historique est vide, initialiser avec la nouvelle valeur
    if (dewPointHistory.empty())
    {
        dewPointHistory.push_back(newDewPoint);
        return newDewPoint;
    }
    
    // Calculer la moyenne actuelle avant d'ajouter la nouvelle valeur
    double currentAverage = 0.0;
    if (dewPointHistory.size() > 0)
    {
        double sum = 0.0;
        for (double value : dewPointHistory)
        {
            sum += value;
        }
        currentAverage = sum / dewPointHistory.size();
    }
    
    // Vérifier si la variation est significative
    double change = std::abs(newDewPoint - currentAverage);
    
    // Si la variation est trop faible, ne pas mettre à jour l'historique
    // mais retourner la moyenne actuelle pour maintenir la stabilité
    if (change < DEW_POINT_MIN_CHANGE && dewPointHistory.size() >= 3)
    {
        return currentAverage;
    }
    
    // Ajouter la nouvelle valeur à l'historique seulement si la variation est significative
    dewPointHistory.push_back(newDewPoint);
    
    // Limiter la taille de l'historique
    if (dewPointHistory.size() > DEW_POINT_FILTER_SIZE)
    {
        dewPointHistory.erase(dewPointHistory.begin());
    }
    
    // Calculer la nouvelle moyenne mobile
    double sum = 0.0;
    for (double value : dewPointHistory)
    {
        sum += value;
    }
    double average = sum / dewPointHistory.size();
    
    return average;
}

double AstrAlimHeater::filterValue(double newValue, std::vector<double>& history, double minChange)
{
    // Si l'historique est vide, initialiser avec la nouvelle valeur
    if (history.empty())
    {
        history.push_back(newValue);
        return newValue;
    }
    
    // Calculer la moyenne actuelle
    double currentAverage = 0.0;
    if (history.size() > 0)
    {
        double sum = 0.0;
        for (double value : history)
        {
            sum += value;
        }
        currentAverage = sum / history.size();
    }
    
    // Vérifier si la variation est significative
    double change = std::abs(newValue - currentAverage);
    
    // Si la variation est trop faible, ne pas mettre à jour l'historique
    if (change < minChange && history.size() >= 3)
    {
        return currentAverage;
    }
    
    // Ajouter la nouvelle valeur à l'historique seulement si la variation est significative
    history.push_back(newValue);
    
    // Limiter la taille de l'historique (même taille que pour le point de rosée)
    if (history.size() > DEW_POINT_FILTER_SIZE)
    {
        history.erase(history.begin());
    }
    
    // Calculer la nouvelle moyenne mobile
    double sum = 0.0;
    for (double value : history)
    {
        sum += value;
    }
    double average = sum / history.size();
    
    return average;
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
        // Heater 1 Power (manual mode)
        if (Heater1PowerNP.isNameMatch(name))
        {
            if (Heater1ModeSP[MODE_MANUAL].getState() == ISS_ON)
            {
                Heater1PowerNP.update(values, names, n);
                setPWMDuty(0, Heater1PowerNP[0].getValue());
                Heater1PowerNP.setState(IPS_OK);
                Heater1PowerNP.apply();
                LOGF_INFO("Heater 1 power set to %.0f%%", Heater1PowerNP[0].getValue());
            }
            return true;
        }
        
        // Heater 2 Power (manual mode)
        if (Heater2PowerNP.isNameMatch(name))
        {
            if (Heater2ModeSP[MODE_MANUAL].getState() == ISS_ON)
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
            else if (Heater1ModeSP[MODE_MANUAL].getState() == ISS_ON)
            {
                Heater1ModeSP.setState(IPS_OK);
                LOG_INFO("Heater 1 set to MANUAL mode");
            }
            else if (Heater1ModeSP[MODE_AUTO].getState() == ISS_ON)
            {
                pidIntegral[0] = 0;
                pidLastError[0] = 0;
                Heater1ModeSP.setState(IPS_BUSY);
                LOG_INFO("Heater 1 set to AUTO (dew point) mode");
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
            else if (Heater2ModeSP[MODE_MANUAL].getState() == ISS_ON)
            {
                Heater2ModeSP.setState(IPS_OK);
                LOG_INFO("Heater 2 set to MANUAL mode");
            }
            else if (Heater2ModeSP[MODE_AUTO].getState() == ISS_ON)
            {
                pidIntegral[1] = 0;
                pidLastError[1] = 0;
                Heater2ModeSP.setState(IPS_BUSY);
                LOG_INFO("Heater 2 set to AUTO (dew point) mode");
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
    Heater1SensorTP.save(fp);
    Heater2SensorTP.save(fp);
    
    return true;
}

std::string AstrAlimHeater::execCommand(const char* cmd)
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
    if (AvailableSensorsSP.count() > 0)
    {
        for (size_t i = 0; i < AvailableSensorsSP.count() && i < availableDS18B20.size(); i++)
        {
            if (AvailableSensorsSP[i].getState() == ISS_ON)
            {
                selectedSensorId = availableDS18B20[i];
                break;
            }
        }
    }
    
    // Vérifier si le nombre de capteurs a changé ou si la propriété n'existe pas encore
    bool needRedefine = (AvailableSensorsSP.count() != sensorCount);
    bool propertyExists = (AvailableSensorsSP.count() > 0);
    
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
    else if (sensorCount > 0 && AvailableSensorsSP.count() == 0)
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
    if (AvailableSensorsSP.count() > 0 && sensorCount > 0)
    {
        for (size_t i = 0; i < sensorCount && i < AvailableSensorsSP.count(); i++)
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
    
    // Vérifier que le heater n'est pas déjà en mode AUTO ou MANUAL avec puissance
    if (modeSP[MODE_AUTO].getState() == ISS_ON || 
        (modeSP[MODE_MANUAL].getState() == ISS_ON && powerNP[0].getValue() > 0))
    {
        LOG_ERROR("Cannot start auto-detect: heater must be in OFF mode or MANUAL with 0% power");
        return false;
    }
    
    // Activer le mode MANUAL temporairement
    modeSP[MODE_OFF].setState(ISS_OFF);
    modeSP[MODE_MANUAL].setState(ISS_ON);
    modeSP[MODE_AUTO].setState(ISS_OFF);
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
        modeSP[MODE_MANUAL].setState(ISS_OFF);
        modeSP[MODE_AUTO].setState(ISS_OFF);
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
    // Read INA219 sensors for heaters (AstraPwm1 and AstraPwm2)
    // Addresses: 0x49 (Heater 1), 0x4d (Heater 2)
    // Format: voltage_mV;current_mA for each heater
    // Use integer payload to avoid locale-dependent float parsing issues.
    std::string result = execCommand(
        "python3 -c \""
        "import os\n"
        "import sys\n"
        "import glob\n"
        "base_paths = [\n"
        "    '/opt/AstraDIY/Software/pythonDrivers',\n"
        "    '/opt/AstraDIY',\n"
        "    '/opt/AstrAlim/Software/pythonDrivers',\n"
        "    '/opt/AstrAlim',\n"
        "    '/root/AstraDIY/Software/pythonDrivers',\n"
        "    '/home/stellarmate/AstraDIY/Software/pythonDrivers',\n"
        "    '/home/stellarmate/Documents/AstraDIY/Software/pythonDrivers',\n"
        "]\n"
        "dynamic_paths = glob.glob('/home/*/AstraDIY/Software/pythonDrivers') + glob.glob('/home/*/Documents/AstraDIY/Software/pythonDrivers')\n"
        "for p in base_paths + dynamic_paths:\n"
        "    if os.path.isdir(p) and p not in sys.path:\n"
        "        sys.path.insert(0, p)\n"
        "try:\n"
        "    from lib.ina219 import INA219\n"
        "    results = []\n"
        "    for addr in [0x49, 0x4d]:\n"
        "        try:\n"
        "            ina = INA219(0.01, 6, busnum=1, address=addr)\n"
        "            ina.configure()\n"
        "            v_mv = max(int(round(ina.voltage() * 1000.0)), 0)\n"
        "            i_ma = max(int(round(abs(ina.current()))), 0)\n"
        "            results.append(f'1;{v_mv};{i_ma}')\n"
        "        except:\n"
        "            results.append('0;0;0')\n"
        "    print('|'.join(results))\n"
        "except Exception as e:\n"
        "    print('0;0;0|0;0;0')\n"
        "\" 2>/dev/null"
    );
    
    if (result.empty())
    {
        PowerMonitorNP.setState(IPS_ALERT);
        PowerMonitorNP.apply();
        return;
    }
    
    // Parse result: ok;v1_mV;i1_mA|ok;v2_mV;i2_mA
    std::istringstream iss(result);
    std::string heaterData;
    
    auto parseSample = [](const std::string& input, bool& valid, double& voltage, double& current)
    {
        int ok = 0;
        long vMilli = 0;
        long cMilli = 0;
        if (sscanf(input.c_str(), "%d;%ld;%ld", &ok, &vMilli, &cMilli) == 3)
        {
            valid = (ok == 1);
            voltage = std::max(0.0, static_cast<double>(vMilli) / 1000.0);
            current = std::max(0.0, static_cast<double>(cMilli) / 1000.0);
            return;
        }
        valid = false;
        voltage = 0.0;
        current = 0.0;
    };

    bool valid1 = false;
    bool valid2 = false;
    double sampleV1 = 0.0;
    double sampleI1 = 0.0;
    double sampleV2 = 0.0;
    double sampleI2 = 0.0;

    // Heater 1 (AstraPwm1, address 0x49, GPIO 18, PWM channel 1)
    if (std::getline(iss, heaterData, '|'))
    {
        parseSample(heaterData, valid1, sampleV1, sampleI1);
    }
    
    // Heater 2 (AstraPwm2, address 0x4d, GPIO 13, PWM channel 2)
    if (std::getline(iss, heaterData, '|'))
    {
        parseSample(heaterData, valid2, sampleV2, sampleI2);
    }

    const bool heater1Active = (Heater1PowerNP[0].getValue() > 1.0);
    const bool heater2Active = (Heater2PowerNP[0].getValue() > 1.0);
    applyINAChannelSample(0, valid1, sampleV1, sampleI1, heater1Active);
    applyINAChannelSample(1, valid2, sampleV2, sampleI2, heater2Active);

    PowerMonitorNP[PWR_VOLTAGE1].setValue(inaDisplayVoltage[0]);
    PowerMonitorNP[PWR_CURRENT1].setValue(inaDisplayCurrent[0]);
    PowerMonitorNP[PWR_VOLTAGE2].setValue(inaDisplayVoltage[1]);
    PowerMonitorNP[PWR_CURRENT2].setValue(inaDisplayCurrent[1]);
    
    // Set state based on whether we have valid readings
    bool hasData = (PowerMonitorNP[PWR_VOLTAGE1].getValue() > 0 || 
                    PowerMonitorNP[PWR_CURRENT1].getValue() > 0 ||
                    PowerMonitorNP[PWR_VOLTAGE2].getValue() > 0 || 
                    PowerMonitorNP[PWR_CURRENT2].getValue() > 0);
    PowerMonitorNP.setState(hasData ? IPS_OK : IPS_IDLE);
    PowerMonitorNP.apply();
}

void AstrAlimHeater::resetINADisplayState()
{
    for (int ch = 0; ch < 2; ch++)
    {
        inaDisplayVoltage[ch] = 0.0;
        inaDisplayCurrent[ch] = 0.0;
        inaHasSample[ch] = false;
        inaInvalidCount[ch] = 0;
        inaZeroWhileActiveCount[ch] = 0;
    }
}

void AstrAlimHeater::applyINAChannelSample(int channel, bool validSample, double sampleVoltage, double sampleCurrent, bool heaterActive)
{
    if (channel < 0 || channel > 1)
        return;

    if (!validSample)
    {
        inaInvalidCount[channel]++;
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

    const bool nearZeroSample = (sampleVoltage < 0.20 && sampleCurrent < 0.02);
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
        inaDisplayCurrent[channel] = sampleCurrent;
        inaHasSample[channel] = true;
        return;
    }

    const double alpha = heaterActive ? INA_FILTER_ALPHA_ACTIVE : INA_FILTER_ALPHA_IDLE;
    inaDisplayVoltage[channel] = (alpha * sampleVoltage) + ((1.0 - alpha) * inaDisplayVoltage[channel]);
    inaDisplayCurrent[channel] = (alpha * sampleCurrent) + ((1.0 - alpha) * inaDisplayCurrent[channel]);

    if (!heaterActive && nearZeroSample)
    {
        if (inaDisplayVoltage[channel] < 0.05)
            inaDisplayVoltage[channel] = 0.0;
        if (inaDisplayCurrent[channel] < 0.01)
            inaDisplayCurrent[channel] = 0.0;
    }
}
