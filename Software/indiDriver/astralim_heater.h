/*******************************************************************************
 * AstrAlim Heater Driver - Dew Heater Controller
 * Copyright (c) 2024 AstrAlim Project
 * 
 * Features:
 * - 2 PWM outputs for dew heaters
 * - 2 DS18B20 1-Wire temperature sensors for heater bands
 * - 1 BME280/BMP280 I2C sensor for ambient temperature/humidity/pressure
 * - Dew point calculation
 * - Automatic PID regulation based on dew point delta
 ******************************************************************************/

#ifndef ASTRALIM_HEATER_H
#define ASTRALIM_HEATER_H

#include <defaultdevice.h>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <array>
#include <condition_variable>
#include <mutex>

namespace AstrAlim {
class GpioController;
}

class AstrAlimHeater : public INDI::DefaultDevice
{
public:
    AstrAlimHeater();
    virtual ~AstrAlimHeater();

    const char* getDefaultName() override;
    
    bool initProperties() override;
    bool updateProperties() override;
    
    bool ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n) override;
    bool ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n) override;
    bool ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n) override;

protected:
    bool Connect() override;
    bool Disconnect() override;
    void TimerHit() override;
    bool saveConfigItems(FILE* fp) override;

private:
    // PWM control
    bool initPWM();
    void closePWM();
    bool setPWMDuty(int channel, double percent);
    void runStepPwmLoop();
    std::array<bool, 10> buildStepPattern(double percent) const;
    
    // Temperature sensors
    bool readDS18B20Sensors();
    bool readBME280();
    double calculateDewPoint(double temp, double humidity);
    std::vector<std::string> scanDS18B20Devices();
    double readDS18B20Temperature(const std::string& sensorId);
    void updateSensorStatusList(bool rescanDevices = true);
    void validateAssignedSensors();
    void autoAssignRemainingSensor(int assignedHeaterChannel, const std::string& assignedSensorId);
    bool autoDetectSensor(int heaterChannel);
    void handleAutoDetect(int heaterChannel);
    bool testSensorResponse(int heaterChannel, const std::string& sensorId);
    double filterDewPoint(double newDewPoint);  // Filtre passe-bas pour le point de rosée
    double filterValue(double newValue, std::vector<double>& history, double minChange);  // Filtre générique pour temp/humidité
    
    // Power monitoring
    void readINA219();
    void resetINADisplayState();
    bool resetINAChannel(int channel);
    void applyINAChannelSample(int channel, bool validSample, bool currentValid, double sampleVoltage, double sampleCurrent, bool heaterActive);
    
    // PID control
    void runPIDControl(int channel);
    double computePID(int channel, double setpoint, double current);
    
    // Helper
    std::string execCommand(const char* cmd);
    
    // Step PWM runtime state (aligned with Python HMI behavior)
    static constexpr int STEP_PWM_TICK_MS = 250;
    static constexpr int STEP_PWM_STEP_COUNT = 10;
    static constexpr int STEP_PWM_GPIO_H1 = 18; // AstraPwm1
    static constexpr int STEP_PWM_GPIO_H2 = 13; // AstraPwm2
    std::unique_ptr<AstrAlim::GpioController> pwmGpio;
    std::array<double, 2> pwmDutyPercent = {0.0, 0.0};
    std::array<std::array<bool, STEP_PWM_STEP_COUNT>, 2> pwmStepPattern = {};
    std::thread pwmStepThread;
    std::atomic<bool> pwmStepRunning {false};
    std::mutex pwmStepMutex;
    std::condition_variable pwmStepCv;
    int pwmStepIndex = 0;
    
    // PID state
    double pidIntegral[2] = {0, 0};
    double pidLastError[2] = {0, 0};
    std::atomic<bool> pidRunning[2];
    std::thread pidThread[2];

    // DS18B20 sensor paths
    std::string ds18b20Path[2];
    std::vector<std::string> availableDS18B20;
    
    // Dew point filtering (moving average with minimum change threshold)
    static constexpr int DEW_POINT_FILTER_SIZE = 10;  // Nombre de valeurs pour la moyenne mobile (augmenté pour plus de stabilité)
    static constexpr double DEW_POINT_MIN_CHANGE = 0.1;  // Variation minimale requise pour mettre à jour (0.1°C)
    static constexpr double TEMP_HUMIDITY_MIN_CHANGE = 0.05;  // Variation minimale pour temp/humidité (0.05°C ou 0.5%)
    std::vector<double> dewPointHistory;  // Historique des points de rosée
    std::vector<double> tempHistory;  // Historique des températures ambiantes
    std::vector<double> humidityHistory;  // Historique des humidités
    double filteredDewPoint;  // Point de rosée filtré
    double filteredTemp;  // Température ambiante filtrée
    double filteredHumidity;  // Humidité filtrée
    
    // Sensor assignment state
    struct SensorAssignState {
        bool autoDetectActive;                    // Test de détection automatique en cours
        std::chrono::steady_clock::time_point testStartTime;
        double testStartTemp[10];                  // Températures de départ pour chaque capteur testé
        int testPower;                             // Puissance utilisée pour le test
    };
    SensorAssignState sensorAssignState[2];

    //========== Properties ==========
    
    // Ambient sensor (BME280)
    INDI::PropertyNumber AmbientNP {4};
    enum { AMB_TEMPERATURE, AMB_HUMIDITY, AMB_PRESSURE, AMB_DEWPOINT };
    
    // Manual humidity input (for BMP280 without humidity sensor)
    INDI::PropertyNumber ManualHumidityNP {1};
    
    // Heater 1 properties
    INDI::PropertyNumber Heater1TempNP {1};      // Current temperature from DS18B20
    INDI::PropertyNumber Heater1PowerNP {1};     // Current PWM duty cycle (0-100%)
    INDI::PropertyNumber Heater1SetpointNP {1};  // Target temperature or delta
    INDI::PropertySwitch Heater1ModeSP {3};      // Off / Manual / Auto
    enum { MODE_OFF, MODE_MANUAL, MODE_AUTO };
    INDI::PropertyText Heater1SensorTP {1};      // Associated DS18B20 sensor ID
    INDI::PropertySwitch Heater1SensorAssignSP {3};  // Auto-Detect / Test / Clear
    enum { SENSOR_ASSIGN_AUTO, SENSOR_ASSIGN_TEST, SENSOR_ASSIGN_CLEAR };
    
    // Heater 2 properties
    INDI::PropertyNumber Heater2TempNP {1};
    INDI::PropertyNumber Heater2PowerNP {1};
    INDI::PropertyNumber Heater2SetpointNP {1};
    INDI::PropertySwitch Heater2ModeSP {3};
    INDI::PropertyText Heater2SensorTP {1};
    INDI::PropertySwitch Heater2SensorAssignSP {3};  // Auto-Detect / Test / Clear
    
    // Available sensors list (dropdown-like using Switch)
    INDI::PropertySwitch AvailableSensorsSP {10};  // Liste déroulante avec Switch (max 10 capteurs)
    INDI::PropertySwitch SensorAssignActionSP {2};  // Assigner le capteur sélectionné
    enum { ASSIGN_TO_HEATER1, ASSIGN_TO_HEATER2 };
    
    // PID parameters (shared)
    INDI::PropertyNumber PIDNP {3};
    enum { PID_KP, PID_KI, PID_KD };
    
    // Dew point delta (target = dewpoint + delta)
    INDI::PropertyNumber DewDeltaNP {1};
    
    // Power monitoring (if INA219 available)
    INDI::PropertyNumber PowerMonitorNP {4};
    enum { PWR_VOLTAGE1, PWR_CURRENT1, PWR_VOLTAGE2, PWR_CURRENT2 };
    std::array<double, 2> inaDisplayVoltage = {0.0, 0.0};
    std::array<double, 2> inaDisplayCurrent = {0.0, 0.0};
    std::array<bool, 2> inaHasSample = {false, false};
    std::array<int, 2> inaInvalidCount = {0, 0};
    std::array<int, 2> inaZeroWhileActiveCount = {0, 0};
    std::array<std::string, 2> inaLastErrorTag = {"", ""};
    std::array<int, 2> inaErrorLogCount = {0, 0};
    std::array<bool, 2> inaNoResponseActive = {false, false};
    std::array<std::chrono::steady_clock::time_point, 2> inaNoResponseSince {};
    std::array<std::chrono::steady_clock::time_point, 2> inaLastResetAttempt {};

    // Constants
    static constexpr int POLL_INTERVAL_MS = 5000;
    static constexpr int SENSOR_LIST_UPDATE_INTERVAL_CYCLES = 6;  // 30s with 5s poll
    static constexpr double INA_FILTER_ALPHA_ACTIVE = 0.25;
    static constexpr double INA_FILTER_ALPHA_IDLE = 0.60;
    static constexpr int INA_ZERO_GLITCH_HOLD_CYCLES = 3;
    static constexpr int INA_INVALID_RESET_CYCLES = 12;
    static constexpr int INA_STARTUP_FALLBACK_CYCLES = 3;
    static constexpr int INA_LOG_REPEAT_CYCLES = 6;
    static constexpr int INA_NO_RESPONSE_RESET_DELAY_MS = 5000;
    static constexpr int INA_RESET_COOLDOWN_MS = 5000;
    static constexpr int INA_ADDR_H1 = 0x49;
    static constexpr int INA_ADDR_H2 = 0x4d;
    static constexpr double DEFAULT_KP = 2.0;
    static constexpr double DEFAULT_KI = 0.1;
    static constexpr double DEFAULT_KD = 0.5;
    static constexpr double DEFAULT_DEW_DELTA = 2.0;
    static constexpr double TEMP_UNAVAILABLE = 100.0;
    static constexpr double DEWPOINT_UNAVAILABLE = -100.0;
};

#endif // ASTRALIM_HEATER_H
