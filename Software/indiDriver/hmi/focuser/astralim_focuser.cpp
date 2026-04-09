/*******************************************************************************
 * AstrAlim Focuser Driver - Modern INDI API Implementation
 * Copyright (c) 2024 AstrAlim Project
 * Based on original work by Radek Kaczorek and D.Germa
 ******************************************************************************/

#include "astralim_focuser.h"
#include "config.h"

#include <cstring>
#include <cmath>
#include <fstream>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

namespace
{
template <typename P>
auto numSetMin(P& prop, int index, double value) -> decltype(prop[index].setMin(value), void())
{
    prop[index].setMin(value);
}

inline void numSetMin(INumberVectorProperty& prop, int index, double value)
{
    prop.np[index].min = value;
}

template <typename P>
auto numSetMax(P& prop, int index, double value) -> decltype(prop[index].setMax(value), void())
{
    prop[index].setMax(value);
}

inline void numSetMax(INumberVectorProperty& prop, int index, double value)
{
    prop.np[index].max = value;
}

template <typename P>
auto numSetStep(P& prop, int index, double value) -> decltype(prop[index].setStep(value), void())
{
    prop[index].setStep(value);
}

inline void numSetStep(INumberVectorProperty& prop, int index, double value)
{
    prop.np[index].step = value;
}

template <typename P>
auto numSetValue(P& prop, int index, double value) -> decltype(prop[index].setValue(value), void())
{
    prop[index].setValue(value);
}

inline void numSetValue(INumberVectorProperty& prop, int index, double value)
{
    prop.np[index].value = value;
}

template <typename P>
auto numGetMin(P& prop, int index) -> decltype(prop[index].getMin())
{
    return prop[index].getMin();
}

inline double numGetMin(INumberVectorProperty& prop, int index)
{
    return prop.np[index].min;
}

template <typename P>
auto numGetMax(P& prop, int index) -> decltype(prop[index].getMax())
{
    return prop[index].getMax();
}

inline double numGetMax(INumberVectorProperty& prop, int index)
{
    return prop.np[index].max;
}

template <typename P>
auto numGetStep(P& prop, int index) -> decltype(prop[index].getStep())
{
    return prop[index].getStep();
}

inline double numGetStep(INumberVectorProperty& prop, int index)
{
    return prop.np[index].step;
}

template <typename P>
auto numGetValue(P& prop, int index) -> decltype(prop[index].getValue())
{
    return prop[index].getValue();
}

inline double numGetValue(INumberVectorProperty& prop, int index)
{
    return prop.np[index].value;
}

template <typename P>
auto numSetState(P& prop, IPState state) -> decltype(prop.setState(state), void())
{
    prop.setState(state);
}

inline void numSetState(INumberVectorProperty& prop, IPState state)
{
    prop.s = state;
}

template <typename P>
auto numApply(P& prop) -> decltype(prop.apply(), void())
{
    prop.apply();
}

inline void numApply(INumberVectorProperty& prop)
{
    IDSetNumber(&prop, nullptr);
}

template <typename P>
auto numIsNameMatch(P& prop, const char* name) -> decltype(prop.isNameMatch(name), bool())
{
    return prop.isNameMatch(name);
}

inline bool numIsNameMatch(INumberVectorProperty& prop, const char* name)
{
    return strcmp(prop.name, name) == 0;
}

template <typename P>
auto numUpdate(P& prop, double values[], char* names[], int n) -> decltype(prop.update(values, names, n), void())
{
    prop.update(values, names, n);
}

inline void numUpdate(INumberVectorProperty& prop, double values[], char* names[], int n)
{
    IUUpdateNumber(&prop, values, names, n);
}

template <typename P>
auto swGetState(P& prop, int index) -> decltype(prop[index].getState())
{
    return prop[index].getState();
}

inline ISState swGetState(ISwitchVectorProperty& prop, int index)
{
    return prop.sp[index].s;
}

template <typename P>
auto swSetState(P& prop, int index, ISState state) -> decltype(prop[index].setState(state), void())
{
    prop[index].setState(state);
}

inline void swSetState(ISwitchVectorProperty& prop, int index, ISState state)
{
    prop.sp[index].s = state;
}
} // namespace

// Sleep macro (milliseconds)
#define msleep(ms) usleep((ms) * 1000)

// Singleton instance
static std::unique_ptr<AstrAlimFocuser> focuserInstance(new AstrAlimFocuser());

AstrAlimFocuser::AstrAlimFocuser()
{
    setVersion(INDI_ASTRALIM_VERSION_MAJOR, INDI_ASTRALIM_VERSION_MINOR);
    
    FI::SetCapability(
        FOCUSER_CAN_ABS_MOVE |
        FOCUSER_CAN_REL_MOVE |
        FOCUSER_CAN_REVERSE  |
        FOCUSER_HAS_BACKLASH |
        FOCUSER_CAN_SYNC     |
        FOCUSER_CAN_ABORT
    );
    
    setSupportedConnections(CONNECTION_NONE);
    
    gpio = std::make_unique<AstrAlim::GpioController>();
}

const char* AstrAlimFocuser::getDefaultName()
{
    return "AstrAlim Focuser";
}

bool AstrAlimFocuser::initProperties()
{
    INDI::Focuser::initProperties();
    
    // Resolution selector
    FocusResolutionSP[RES_1].fill("FOCUS_RESOLUTION_1", "Full Step", ISS_ON);
    FocusResolutionSP[RES_2].fill("FOCUS_RESOLUTION_2", "1/2 Step", ISS_OFF);
    FocusResolutionSP[RES_4].fill("FOCUS_RESOLUTION_4", "1/4 Step", ISS_OFF);
    FocusResolutionSP[RES_8].fill("FOCUS_RESOLUTION_8", "1/8 Step", ISS_OFF);
    FocusResolutionSP[RES_16].fill("FOCUS_RESOLUTION_16", "1/16 Step", ISS_OFF);
    FocusResolutionSP[RES_32].fill("FOCUS_RESOLUTION_32", "1/32 Step", ISS_OFF);
    FocusResolutionSP.fill(getDeviceName(), "FOCUS_RESOLUTION", "Resolution", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Motor board selector
    MotorBoardSP[BOARD_DRV8834].fill("DRV8834", "DRV8834", ISS_ON);
    MotorBoardSP[BOARD_A4988].fill("A4988", "A4988", ISS_OFF);
    MotorBoardSP.fill(getDeviceName(), "MOTOR_BOARD", "Control Board", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Stepper standby
    StepperStandbySP[STANDBY_ENABLED].fill("STEPPER_STANDBY_ON", "Enable", ISS_ON);
    StepperStandbySP[STANDBY_DISABLED].fill("STEPPER_STANDBY_OFF", "Disable", ISS_OFF);
    StepperStandbySP.fill(getDeviceName(), "STEPPER_STANDBY", "Standby", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Standby delay
    StepperStandbyTimeNP[0].fill("STEPPER_STANDBY_DELAY_VALUE", "seconds", "%0.0f", 0, 600, 10, 60);
    StepperStandbyTimeNP.fill(getDeviceName(), "STEPPER_STANDBY_DELAY", "Standby Delay", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // Step delay
    FocusStepDelayNP[0].fill("FOCUS_STEPDELAY_VALUE", "milliseconds", "%0.0f", 1, 10, 1, 1);
    FocusStepDelayNP.fill(getDeviceName(), "FOCUS_STEPDELAY", "Step Delay", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // Focuser travel
    FocuserTravelNP[0].fill("FOCUSER_TRAVEL_VALUE", "mm", "%0.0f", 10, 200, 10, 10);
    FocuserTravelNP.fill(getDeviceName(), "FOCUSER_TRAVEL", "Max Travel", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    
    // Focuser info (read-only)
    FocuserInfoNP[0].fill("CFZ_STEP_ACT", "Step Size (μm)", "%0.2f", 0, 1000, 1, 0);
    FocuserInfoNP[1].fill("CFZ", "Critical Focus Zone (μm)", "%0.2f", 0, 1000, 1, 0);
    FocuserInfoNP[2].fill("STEPS_PER_CFZ", "Steps / CFZ", "%0.0f", 0, 1000, 1, 0);
    FocuserInfoNP.fill(getDeviceName(), "FOCUSER_PARAMETERS", "Focuser Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Temperature
    FocusTemperatureNP[0].fill("FOCUS_TEMPERATURE_VALUE", "°C", "%0.2f", -50, 50, 1, 0);
    FocusTemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Temperature coefficient
    TemperatureCoefNP[0].fill("TEMP_COEF", "μm/m°C", "%.1f", 0, 50, 1, 0);
    TemperatureCoefNP.fill(getDeviceName(), "TEMPERATURE_COEFFICIENT", "Temp Coefficient", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    
    // Temperature compensation switch
    TemperatureCompensateSP[TC_ENABLED].fill("TC_ENABLE", "Enable", ISS_OFF);
    TemperatureCompensateSP[TC_DISABLED].fill("TC_DISABLE", "Disable", ISS_ON);
    TemperatureCompensateSP.fill(getDeviceName(), "TEMPERATURE_COMPENSATE", "Temp Compensate", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Active telescope for snooping
    ActiveTelescopeTP[0].fill("ACTIVE_TELESCOPE_NAME", "Telescope", "Telescope Simulator");
    ActiveTelescopeTP.fill(getDeviceName(), "ACTIVE_TELESCOPE", "Snoop Devices", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    
    // Scope parameters (snooped)
    ScopeParametersNP[0].fill("TELESCOPE_APERTURE", "Aperture (mm)", "%g", 10, 5000, 0, 0);
    ScopeParametersNP[1].fill("TELESCOPE_FOCAL_LENGTH", "Focal Length (mm)", "%g", 10, 10000, 0, 0);
    ScopeParametersNP.fill(ActiveTelescopeTP[0].getText(), "TELESCOPE_INFO", "Scope Properties", OPTIONS_TAB, IP_RW, 60, IPS_OK);
    
    // Set initial position limits
    numSetMin(FocusMaxPosNP, 0, MINMAX_MIN_POS);
    numSetMax(FocusMaxPosNP, 0, MINMAX_MAX_POS);
    numSetStep(FocusMaxPosNP, 0, MINMAX_MAX_POS / 100);
    numSetValue(FocusMaxPosNP, 0, MINMAX_MAX_POS / 10);

    numSetMin(FocusAbsPosNP, 0, 0);
    numSetMax(FocusAbsPosNP, 0, numGetValue(FocusMaxPosNP, 0));
    numSetStep(FocusAbsPosNP, 0, numGetMax(FocusAbsPosNP, 0) / 100);

    numSetMin(FocusRelPosNP, 0, 0);
    numSetMax(FocusRelPosNP, 0, numGetMax(FocusAbsPosNP, 0) / 10);
    numSetStep(FocusRelPosNP, 0, numGetMax(FocusRelPosNP, 0) / 10);
    numSetValue(FocusRelPosNP, 0, numGetMax(FocusRelPosNP, 0) / 10);

    numSetMin(FocusSyncNP, 0, 0);
    numSetMax(FocusSyncNP, 0, numGetMax(FocusAbsPosNP, 0));
    numSetStep(FocusSyncNP, 0, numGetMax(FocusAbsPosNP, 0) / 100);

    numSetMin(FocusBacklashNP, 0, 0);
    numSetMax(FocusBacklashNP, 0, numGetMax(FocusAbsPosNP, 0) / 100);
    numSetStep(FocusBacklashNP, 0, numGetMax(FocusBacklashNP, 0) / 100);
    
    // Default direction
    swSetState(FocusMotionSP, FOCUS_OUTWARD, ISS_ON);
    swSetState(FocusMotionSP, FOCUS_INWARD, ISS_OFF);
    
    // Add debug control
    addDebugControl();
    addConfigurationControl();
    setDefaultPollingPeriod(1000);
    
    // Define motor board before connecting (needs to be set first)
    defineProperty(MotorBoardSP);
    
    // Load config for motor board
    loadConfig(true, "MOTOR_BOARD");
    
    return true;
}

bool AstrAlimFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();
    
    if (isConnected())
    {
        defineProperty(FocusResolutionSP);
        defineProperty(StepperStandbySP);
        defineProperty(StepperStandbyTimeNP);
        defineProperty(FocusStepDelayNP);
        defineProperty(FocuserTravelNP);
        defineProperty(FocuserInfoNP);
        defineProperty(ActiveTelescopeTP);
        
        // Snoop telescope
        IDSnoopDevice(ActiveTelescopeTP[0].getText(), "TELESCOPE_INFO");
        
        // Check for temperature sensor
        if (readDS18B20())
        {
            defineProperty(FocusTemperatureNP);
            defineProperty(TemperatureCoefNP);
            defineProperty(TemperatureCompensateSP);
            
            lastTemperature = FocusTemperatureNP[0].getValue();
            
            // Start temperature update timer
            updateTemperatureID = IEAddTimer(TEMPERATURE_UPDATE_TIMEOUT, updateTemperatureHelper, this);
            temperatureCompensationID = IEAddTimer(TEMPERATURE_COMPENSATION_TIMEOUT, temperatureCompensationHelper, this);
        }
    }
    else
    {
        deleteProperty(FocusResolutionSP);
        deleteProperty(StepperStandbySP);
        deleteProperty(StepperStandbyTimeNP);
        deleteProperty(FocusStepDelayNP);
        deleteProperty(FocuserTravelNP);
        deleteProperty(FocuserInfoNP);
        deleteProperty(ActiveTelescopeTP);
        deleteProperty(FocusTemperatureNP);
        deleteProperty(TemperatureCoefNP);
        deleteProperty(TemperatureCompensateSP);
    }
    
    return true;
}

bool AstrAlimFocuser::Connect()
{
    if (!gpio->openChip(AstrAlim::RPI5_GPIO_CHIP))
    {
        LOG_ERROR("Failed to open GPIO chip. Is this running on a Raspberry Pi 5?");
        return false;
    }
    
    // Check if pins are available
    std::vector<int> pins = {
        AstrAlim::FocuserPins::DIR,
        AstrAlim::FocuserPins::STEP,
        AstrAlim::FocuserPins::SLEEP,
        AstrAlim::FocuserPins::M1,
        AstrAlim::FocuserPins::M2
    };
    
    if (MotorBoardSP[BOARD_A4988].getState() == ISS_ON)
    {
        pins.push_back(AstrAlim::FocuserPins::M3);
    }
    
    for (int pin : pins)
    {
        if (gpio->isLineUsed(pin))
        {
            std::string consumer = gpio->getLineConsumer(pin);
            if (!consumer.empty())
            {
                LOGF_ERROR("GPIO pin %d is already in use by '%s'. "
                          "Please stop any process using this GPIO before connecting.",
                          pin, consumer.c_str());
            }
            else
            {
                LOGF_ERROR("GPIO pin %d is already in use by another process. "
                          "Please stop any process using this GPIO before connecting.",
                          pin);
            }
            gpio->closeChip();
            return false;
        }
    }
    
    // Request GPIO lines
    if (!gpio->requestOutput(AstrAlim::FocuserPins::DIR, "astralim_focuser", 1) ||
        !gpio->requestOutput(AstrAlim::FocuserPins::STEP, "astralim_focuser", 0) ||
        !gpio->requestOutput(AstrAlim::FocuserPins::SLEEP, "astralim_focuser", 1) ||
        !gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0) ||
        !gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0))
    {
        LOG_ERROR("Failed to request GPIO lines");
        gpio->closeChip();
        return false;
    }
    
    if (MotorBoardSP[BOARD_A4988].getState() == ISS_ON)
    {
        if (!gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 0))
        {
            LOG_ERROR("Failed to request M3 GPIO line");
            gpio->closeChip();
            return false;
        }
    }
    
    motorAwake = true;
    
    // Load saved position
    int savedPos = loadPosition();
    if (savedPos >= 0)
    {
        numSetValue(FocusAbsPosNP, 0, savedPos * resolution / MAX_RESOLUTION);
    }
    else
    {
        numSetValue(FocusAbsPosNP, 0, 0);
    }
    
    // Set resolution
    setResolution(resolution);
    
    // Lock motor board setting while connected
    MotorBoardSP.setState(IPS_BUSY);
    MotorBoardSP.apply();
    
    // Update focuser info
    updateFocuserInfo();
    
    // Setup standby timer if enabled
    if (StepperStandbySP[STANDBY_ENABLED].getState() == ISS_ON)
    {
        stepperStandbyID = IEAddTimer(static_cast<int>(StepperStandbyTimeNP[0].getValue()) * 1000, stepperStandbyHelper, this);
        LOGF_INFO("Focuser going standby in %.0f seconds", StepperStandbyTimeNP[0].getValue());
    }
    
    LOG_INFO("AstrAlim Focuser connected successfully");
    return true;
}

bool AstrAlimFocuser::Disconnect()
{
    // Stop timers
    if (stepperStandbyID >= 0) IERmTimer(stepperStandbyID);
    if (updateTemperatureID >= 0) IERmTimer(updateTemperatureID);
    if (temperatureCompensationID >= 0) IERmTimer(temperatureCompensationID);
    
    stepperStandbyID = -1;
    updateTemperatureID = -1;
    temperatureCompensationID = -1;
    
    // Put motor to sleep
    sleepMotor();
    
    // Close GPIO
    gpio->closeChip();
    
    // Unlock motor board setting
    MotorBoardSP.setState(IPS_IDLE);
    MotorBoardSP.apply();
    
    LOG_INFO("AstrAlim Focuser disconnected");
    return true;
}

IPState AstrAlimFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    if (backlashTicksRemaining > 0 || focuserTicksRemaining > 0)
    {
        LOG_WARN("Focuser movement still in progress");
        return IPS_BUSY;
    }
    
    if (targetTicks < numGetMin(FocusAbsPosNP, 0) || targetTicks > numGetMax(FocusAbsPosNP, 0))
    {
        LOG_WARN("Requested position is out of range");
        return IPS_ALERT;
    }
    
    if (targetTicks == static_cast<uint32_t>(numGetValue(FocusAbsPosNP, 0)))
    {
        LOG_INFO("Already at requested position");
        return IPS_OK;
    }
    
    // Wake up motor
    wakeUpMotor();
    
    // Determine direction
    int newDirection;
    const char* directionName;
    
    if (targetTicks > numGetValue(FocusAbsPosNP, 0))
    {
        newDirection = 1;
        directionName = "outward";
    }
    else
    {
        newDirection = -1;
        directionName = "inward";
    }
    
    // Handle backlash if direction changed
    if (newDirection != stepperDirection && numGetValue(FocusBacklashNP, 0) != 0 && swGetState(FocusBacklashSP, INDI_ENABLED) == ISS_ON)
    {
        LOGF_INFO("Compensating backlash by %.0f steps", numGetValue(FocusBacklashNP, 0));
        backlashTicksRemaining = static_cast<int>(numGetValue(FocusBacklashNP, 0));
    }
    else
    {
        backlashTicksRemaining = 0;
    }
    
    stepperDirection = newDirection;
    focuserTicksRemaining = std::abs(static_cast<int>(targetTicks) - static_cast<int>(numGetValue(FocusAbsPosNP, 0)));
    
    LOGF_INFO("Moving focuser %s to position %d", directionName, targetTicks);
    
    SetTimer(static_cast<uint32_t>(FocusStepDelayNP[0].getValue()));
    
    return IPS_BUSY;
}

IPState AstrAlimFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPos = static_cast<int32_t>(numGetValue(FocusAbsPosNP, 0)) + (ticks * (dir == FOCUS_INWARD ? -1 : 1));
    return MoveAbsFocuser(static_cast<uint32_t>(std::max(0, newPos)));
}

bool AstrAlimFocuser::ReverseFocuser(bool enabled)
{
    LOGF_INFO("Reverse direction %s", enabled ? "enabled" : "disabled");
    return true;
}

bool AstrAlimFocuser::SyncFocuser(uint32_t ticks)
{
    savePosition(static_cast<int>(ticks) * MAX_RESOLUTION / resolution);
    LOGF_INFO("Focuser synced to position %d", ticks);
    return true;
}

bool AstrAlimFocuser::SetFocuserBacklash(int32_t steps)
{
    LOGF_INFO("Backlash compensation set to %d steps", steps);
    return true;
}

bool AstrAlimFocuser::AbortFocuser()
{
    backlashTicksRemaining = 0;
    focuserTicksRemaining = 0;
    LOG_INFO("Focuser motion aborted");
    return true;
}

void AstrAlimFocuser::TimerHit()
{
    if (!isConnected())
        return;
    
    if (backlashTicksRemaining == 0 && focuserTicksRemaining == 0)
    {
        // Save position
        savePosition(static_cast<int>(numGetValue(FocusAbsPosNP, 0)) * MAX_RESOLUTION / resolution);

        numSetState(FocusAbsPosNP, IPS_OK);
        numApply(FocusAbsPosNP);
        numSetState(FocusRelPosNP, IPS_OK);
        numApply(FocusRelPosNP);

        LOGF_INFO("Focuser at position %.0f", numGetValue(FocusAbsPosNP, 0));
        
        // Reset temperature reference
        lastTemperature = FocusTemperatureNP[0].getValue();
        
        // Setup standby timer
        if (StepperStandbySP[STANDBY_ENABLED].getState() == ISS_ON)
        {
            if (stepperStandbyID >= 0) IERmTimer(stepperStandbyID);
            stepperStandbyID = IEAddTimer(static_cast<int>(StepperStandbyTimeNP[0].getValue()) * 1000, stepperStandbyHelper, this);
        }
        
        return;
    }
    
    // Set direction GPIO
    int dirValue;
    if (stepperDirection == 1)
    {
        dirValue = (swGetState(FocusReverseSP, INDI_ENABLED) == ISS_ON) ? 0 : 1;
    }
    else
    {
        dirValue = (swGetState(FocusReverseSP, INDI_ENABLED) == ISS_ON) ? 1 : 0;
    }
    gpio->setValue(AstrAlim::FocuserPins::DIR, dirValue);
    
    bool isBacklash = (backlashTicksRemaining > 0);
    
    // Make one step
    stepMotor();
    
    if (isBacklash)
    {
        backlashTicksRemaining--;
    }
    else
    {
        focuserTicksRemaining--;
        numSetValue(FocusAbsPosNP, 0, numGetValue(FocusAbsPosNP, 0) + stepperDirection);
        numApply(FocusAbsPosNP);
    }
    
    SetTimer(static_cast<uint32_t>(FocusStepDelayNP[0].getValue()));
}

void AstrAlimFocuser::stepMotor()
{
    gpio->setValue(AstrAlim::FocuserPins::STEP, 1);
    msleep(static_cast<int>(FocusStepDelayNP[0].getValue()));
    gpio->setValue(AstrAlim::FocuserPins::STEP, 0);
}

void AstrAlimFocuser::wakeUpMotor()
{
    if (!motorAwake)
    {
        if (stepperStandbyID >= 0)
        {
            IERmTimer(stepperStandbyID);
            stepperStandbyID = -1;
        }
        gpio->setValue(AstrAlim::FocuserPins::SLEEP, 1);
        motorAwake = true;
        LOG_INFO("Stepper motor waking up");
    }
}

void AstrAlimFocuser::sleepMotor()
{
    gpio->setValue(AstrAlim::FocuserPins::SLEEP, 0);
    motorAwake = false;
}

void AstrAlimFocuser::setResolution(int res)
{
    // Release M1, M2, M3 lines first
    gpio->releaseLine(AstrAlim::FocuserPins::M1);
    gpio->releaseLine(AstrAlim::FocuserPins::M2);
    gpio->releaseLine(AstrAlim::FocuserPins::M3);
    
    if (MotorBoardSP[BOARD_DRV8834].getState() == ISS_ON)
    {
        // DRV8834 resolution settings
        switch (res)
        {
            case 1:  // Full step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                break;
            case 2:  // 1/2 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                break;
            case 4:  // 1/4 step (M1 floating)
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                break;
            case 8:  // 1/8 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 1);
                break;
            case 16: // 1/16 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 1);
                break;
            case 32: // 1/32 step (M1 floating)
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 1);
                break;
            default:
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                break;
        }
    }
    else // A4988
    {
        switch (res)
        {
            case 1:  // Full step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 0);
                break;
            case 2:  // 1/2 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 0);
                break;
            case 4:  // 1/4 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 0);
                break;
            case 8:  // 1/8 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 0);
                break;
            case 16: // 1/16 step
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 1);
                gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 1);
                break;
            default:
                gpio->requestOutput(AstrAlim::FocuserPins::M1, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M2, "astralim_focuser", 0);
                gpio->requestOutput(AstrAlim::FocuserPins::M3, "astralim_focuser", 0);
                break;
        }
    }
}

int AstrAlimFocuser::savePosition(int pos)
{
    char posFileName[MAXRBUF];
    
    if (getenv("INDICONFIG"))
    {
        snprintf(posFileName, MAXRBUF, "%s.position", getenv("INDICONFIG"));
    }
    else
    {
        snprintf(posFileName, MAXRBUF, "%s/.indi/%s.position", getenv("HOME"), getDeviceName());
    }
    
    std::ofstream file(posFileName);
    if (!file.is_open())
    {
        LOGF_ERROR("Failed to save position to %s", posFileName);
        return -1;
    }
    
    file << pos;
    file.close();
    
    LOGF_DEBUG("Position %d saved to %s", pos, posFileName);
    return pos;
}

int AstrAlimFocuser::loadPosition()
{
    char posFileName[MAXRBUF];
    
    if (getenv("INDICONFIG"))
    {
        snprintf(posFileName, MAXRBUF, "%s.position", getenv("INDICONFIG"));
    }
    else
    {
        snprintf(posFileName, MAXRBUF, "%s/.indi/%s.position", getenv("HOME"), getDeviceName());
    }
    
    std::ifstream file(posFileName);
    if (!file.is_open())
    {
        LOGF_DEBUG("No saved position file found at %s", posFileName);
        return -1;
    }
    
    int pos;
    file >> pos;
    file.close();
    
    LOGF_DEBUG("Position %d loaded from %s", pos, posFileName);
    return pos;
}

bool AstrAlimFocuser::readDS18B20()
{
    DIR* dir;
    struct dirent* dirent;
    char dev[16] = {0};
    char devPath[128];
    char buf[256];
    const char* path = "/sys/bus/w1/devices";
    
    dir = opendir(path);
    if (!dir)
    {
        LOG_DEBUG("1-Wire interface not available");
        return false;
    }
    
    // Find DS18B20 sensor (starts with 28-)
    while ((dirent = readdir(dir)))
    {
        if (dirent->d_type == DT_LNK && strstr(dirent->d_name, "28-") != nullptr)
        {
            strncpy(dev, dirent->d_name, sizeof(dev) - 1);
            break;
        }
    }
    closedir(dir);
    
    if (dev[0] == '\0')
    {
        LOG_DEBUG("No DS18B20 sensor found");
        return false;
    }
    
    snprintf(devPath, sizeof(devPath), "%s/%s/w1_slave", path, dev);
    
    int fd = open(devPath, O_RDONLY);
    if (fd == -1)
    {
        LOG_DEBUG("Cannot open temperature sensor");
        return false;
    }
    
    ssize_t numRead;
    while ((numRead = read(fd, buf, sizeof(buf) - 1)) > 0);
    close(fd);
    
    buf[sizeof(buf) - 1] = '\0';
    
    char* tempStr = strstr(buf, "t=");
    if (!tempStr)
    {
        LOG_DEBUG("Invalid temperature data");
        return false;
    }
    
    float tempC = strtof(tempStr + 2, nullptr) / 1000.0f;
    
    if (std::abs(tempC) > 100)
    {
        LOG_DEBUG("Temperature reading out of range");
        return false;
    }
    
    FocusTemperatureNP[0].setValue(tempC);
    FocusTemperatureNP.setState(IPS_OK);
    FocusTemperatureNP.apply();
    
    LOGF_DEBUG("Temperature: %.2f°C", tempC);
    return true;
}

void AstrAlimFocuser::updateFocuserInfo()
{
    float travel_mm = FocuserTravelNP[0].getValue();
    float aperture = ScopeParametersNP[0].getValue();
    float focal = ScopeParametersNP[1].getValue();
    float f_ratio = 0;
    
    if (aperture > 0 && focal > 0)
    {
        f_ratio = focal / aperture;
    }
    else
    {
        LOG_WARN("No telescope info available for CFZ calculation");
    }
    
    float cfz = 4.88f * 0.520f * f_ratio * f_ratio;
    float step_size = 1000.0f * travel_mm / numGetValue(FocusMaxPosNP, 0);
    float steps_per_cfz = (step_size > 0) ? cfz / step_size : 0;
    
    FocuserInfoNP[0].setValue(step_size);
    FocuserInfoNP[1].setValue(cfz);
    FocuserInfoNP[2].setValue(steps_per_cfz);
    
    if (steps_per_cfz >= 4)
    {
        FocuserInfoNP.setState(IPS_OK);
    }
    else if (steps_per_cfz > 2)
    {
        FocuserInfoNP.setState(IPS_BUSY);
        LOG_WARN("Resolution may be too low for optimal focusing");
    }
    else
    {
        FocuserInfoNP.setState(IPS_ALERT);
        LOG_WARN("Resolution is too low for critical focus zone");
    }
    
    FocuserInfoNP.apply();
}

bool AstrAlimFocuser::ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Standby delay
        if (StepperStandbyTimeNP.isNameMatch(name))
        {
            StepperStandbyTimeNP.update(values, names, n);
            StepperStandbyTimeNP.setState(IPS_OK);
            StepperStandbyTimeNP.apply();
            LOGF_INFO("Standby delay set to %.0f seconds", StepperStandbyTimeNP[0].getValue());
            return true;
        }
        
        // Step delay
        if (FocusStepDelayNP.isNameMatch(name))
        {
            FocusStepDelayNP.update(values, names, n);
            FocusStepDelayNP.setState(IPS_OK);
            FocusStepDelayNP.apply();
            LOGF_INFO("Step delay set to %.0f ms", FocusStepDelayNP[0].getValue());
            return true;
        }
        
        // Focuser travel
        if (FocuserTravelNP.isNameMatch(name))
        {
            FocuserTravelNP.update(values, names, n);
            FocuserTravelNP.setState(IPS_OK);
            FocuserTravelNP.apply();
            updateFocuserInfo();
            LOGF_INFO("Max travel set to %.0f mm", FocuserTravelNP[0].getValue());
            return true;
        }
        
        // Temperature coefficient
        if (TemperatureCoefNP.isNameMatch(name))
        {
            TemperatureCoefNP.update(values, names, n);
            TemperatureCoefNP.setState(IPS_OK);
            TemperatureCoefNP.apply();
            LOGF_INFO("Temperature coefficient set to %.1f μm/m°C", TemperatureCoefNP[0].getValue());
            return true;
        }
        
        // Max position changed
        if (numIsNameMatch(FocusMaxPosNP, name))
        {
            numUpdate(FocusMaxPosNP, values, names, n);
            updateFocuserInfo();
            numApply(FocusMaxPosNP);
        }
    }
    
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool AstrAlimFocuser::ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // Motor board
        if (MotorBoardSP.isNameMatch(name))
        {
            if (isConnected())
            {
                LOG_WARN("Cannot change motor board while connected");
                return false;
            }
            
            MotorBoardSP.update(states, names, n);
            MotorBoardSP.setState(IPS_OK);
            MotorBoardSP.apply();
            
            if (MotorBoardSP[BOARD_DRV8834].getState() == ISS_ON)
                LOG_INFO("Motor board set to DRV8834");
            else
                LOG_INFO("Motor board set to A4988");
            
            return true;
        }
        
        // Stepper standby
        if (StepperStandbySP.isNameMatch(name))
        {
            StepperStandbySP.update(states, names, n);
            StepperStandbySP.setState(IPS_OK);
            StepperStandbySP.apply();
            
            if (StepperStandbySP[STANDBY_ENABLED].getState() == ISS_ON)
                LOG_INFO("Stepper standby enabled");
            else
                LOG_INFO("Stepper standby disabled");
            
            return true;
        }
        
        // Resolution
        if (FocusResolutionSP.isNameMatch(name))
        {
            int lastResolution = resolution;
            int currentSwitch = FocusResolutionSP.findOnSwitchIndex();
            
            FocusResolutionSP.update(states, names, n);
            
            int newSwitch = FocusResolutionSP.findOnSwitchIndex();
            
            // Map switch index to resolution
            int resolutions[] = {1, 2, 4, 8, 16, 32};
            int newResolution = resolutions[newSwitch];
            
            // A4988 doesn't support 1/32
            if (newResolution == 32 && MotorBoardSP[BOARD_A4988].getState() == ISS_ON)
            {
                FocusResolutionSP.reset();
                FocusResolutionSP[currentSwitch].setState(ISS_ON);
                FocusResolutionSP.apply();
                LOG_WARN("A4988 does not support 1/32 resolution");
                return false;
            }
            
            resolution = newResolution;
            setResolution(resolution);
            
            // Update all position-related values
            double ratio = static_cast<double>(resolution) / lastResolution;
            
            numSetMax(FocusMaxPosNP, 0, numGetMax(FocusMaxPosNP, 0) * ratio);
            numSetStep(FocusMaxPosNP, 0, numGetStep(FocusMaxPosNP, 0) * ratio);
            numSetValue(FocusMaxPosNP, 0, numGetValue(FocusMaxPosNP, 0) * ratio);

            numSetMax(FocusAbsPosNP, 0, numGetMax(FocusAbsPosNP, 0) * ratio);
            numSetStep(FocusAbsPosNP, 0, numGetStep(FocusAbsPosNP, 0) * ratio);
            numSetValue(FocusAbsPosNP, 0, numGetValue(FocusAbsPosNP, 0) * ratio);

            numSetMax(FocusRelPosNP, 0, numGetMax(FocusRelPosNP, 0) * ratio);
            numSetStep(FocusRelPosNP, 0, numGetStep(FocusRelPosNP, 0) * ratio);
            numSetValue(FocusRelPosNP, 0, numGetValue(FocusRelPosNP, 0) * ratio);

            numSetMax(FocusSyncNP, 0, numGetMax(FocusSyncNP, 0) * ratio);
            numSetStep(FocusSyncNP, 0, numGetStep(FocusSyncNP, 0) * ratio);

            numSetMax(FocusBacklashNP, 0, numGetMax(FocusBacklashNP, 0) * ratio);
            numSetStep(FocusBacklashNP, 0, numGetStep(FocusBacklashNP, 0) * ratio);
            numSetValue(FocusBacklashNP, 0, numGetValue(FocusBacklashNP, 0) * ratio);

            numApply(FocusMaxPosNP);
            numApply(FocusAbsPosNP);
            numApply(FocusRelPosNP);
            numApply(FocusSyncNP);
            numApply(FocusBacklashNP);
            
            updateFocuserInfo();
            
            FocusResolutionSP.setState(IPS_OK);
            FocusResolutionSP.apply();
            
            LOGF_INFO("Resolution set to 1/%d", resolution);
            return true;
        }
        
        // Temperature compensation
        if (TemperatureCompensateSP.isNameMatch(name))
        {
            TemperatureCompensateSP.update(states, names, n);
            
            if (TemperatureCompensateSP[TC_ENABLED].getState() == ISS_ON)
            {
                TemperatureCompensateSP.setState(IPS_OK);
                LOG_INFO("Temperature compensation enabled");
            }
            else
            {
                TemperatureCompensateSP.setState(IPS_IDLE);
                LOG_INFO("Temperature compensation disabled");
            }
            
            TemperatureCompensateSP.apply();
            return true;
        }
    }
    
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool AstrAlimFocuser::ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        if (ActiveTelescopeTP.isNameMatch(name))
        {
            ActiveTelescopeTP.update(texts, names, n);
            ActiveTelescopeTP.setState(IPS_OK);
            ActiveTelescopeTP.apply();
            
            // Update snoop target
            ScopeParametersNP.setDeviceName(ActiveTelescopeTP[0].getText());
            IDSnoopDevice(ActiveTelescopeTP[0].getText(), "TELESCOPE_INFO");
            
            LOGF_INFO("Active telescope set to %s", ActiveTelescopeTP[0].getText());
            return true;
        }
    }
    
    return INDI::Focuser::ISNewText(dev, name, texts, names, n);
}

bool AstrAlimFocuser::ISSnoopDevice(XMLEle* root)
{
    // Use the new INDI 2.x snooping mechanism
    const char* propName = findXMLAttValu(root, "name");
    const char* deviceName = findXMLAttValu(root, "device");
    
    if (propName && deviceName && strcmp(propName, "TELESCOPE_INFO") == 0)
    {
        XMLEle* ep = nullptr;
        for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
        {
            const char* elemName = findXMLAttValu(ep, "name");
            if (elemName)
            {
                if (strcmp(elemName, "TELESCOPE_APERTURE") == 0)
                    ScopeParametersNP[0].setValue(atof(pcdataXMLEle(ep)));
                else if (strcmp(elemName, "TELESCOPE_FOCAL_LENGTH") == 0)
                    ScopeParametersNP[1].setValue(atof(pcdataXMLEle(ep)));
            }
        }
        updateFocuserInfo();
        return true;
    }
    
    return INDI::Focuser::ISSnoopDevice(root);
}

bool AstrAlimFocuser::saveConfigItems(FILE* fp)
{
    INDI::Focuser::saveConfigItems(fp);
    
    MotorBoardSP.save(fp);
    StepperStandbySP.save(fp);
    StepperStandbyTimeNP.save(fp);
    FocusResolutionSP.save(fp);
    FocusStepDelayNP.save(fp);
    FocuserTravelNP.save(fp);
    TemperatureCompensateSP.save(fp);
    TemperatureCoefNP.save(fp);
    ActiveTelescopeTP.save(fp);
    
    return true;
}

void AstrAlimFocuser::stepperStandbyHelper(void* context)
{
    static_cast<AstrAlimFocuser*>(context)->stepperStandby();
}

void AstrAlimFocuser::updateTemperatureHelper(void* context)
{
    static_cast<AstrAlimFocuser*>(context)->updateTemperature();
}

void AstrAlimFocuser::temperatureCompensationHelper(void* context)
{
    static_cast<AstrAlimFocuser*>(context)->temperatureCompensation();
}

void AstrAlimFocuser::stepperStandby()
{
    if (!isConnected())
        return;
    
    sleepMotor();
    LOG_INFO("Stepper motor entering standby");
}

void AstrAlimFocuser::updateTemperature()
{
    if (!isConnected())
        return;
    
    readDS18B20();
    updateTemperatureID = IEAddTimer(TEMPERATURE_UPDATE_TIMEOUT, updateTemperatureHelper, this);
}

void AstrAlimFocuser::temperatureCompensation()
{
    if (!isConnected())
        return;
    
    if (TemperatureCompensateSP[TC_ENABLED].getState() == ISS_ON && 
        FocusTemperatureNP[0].getValue() != lastTemperature)
    {
        float deltaTemp = FocusTemperatureNP[0].getValue() - lastTemperature;
        float thermalExpansionRatio = TemperatureCoefNP[0].getValue() * ScopeParametersNP[1].getValue() / 1000.0f;
        float thermalExpansion = thermalExpansionRatio * deltaTemp;
        
        LOGF_DEBUG("Thermal expansion: %.1f μm due to %.2f°C change", thermalExpansion, deltaTemp);
        
        if (std::abs(thermalExpansion) > FocuserInfoNP[1].getValue() / 2)
        {
            int adjustment = static_cast<int>(std::round((thermalExpansion / FocuserInfoNP[0].getValue()) / 2));
            MoveAbsFocuser(static_cast<uint32_t>(numGetValue(FocusAbsPosNP, 0) + adjustment));
            lastTemperature = FocusTemperatureNP[0].getValue();
            LOGF_INFO("Focuser adjusted by %d steps due to %.2f°C temperature change", adjustment, deltaTemp);
        }
    }
    
    temperatureCompensationID = IEAddTimer(TEMPERATURE_COMPENSATION_TIMEOUT, temperatureCompensationHelper, this);
}
