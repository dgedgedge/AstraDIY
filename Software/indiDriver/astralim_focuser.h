/*******************************************************************************
 * AstrAlim Focuser Driver - Modern INDI API
 * Copyright (c) 2024 AstrAlim Project
 * Based on original work by Radek Kaczorek and D.Germa
 ******************************************************************************/

#ifndef ASTRALIM_FOCUSER_H
#define ASTRALIM_FOCUSER_H

#include <indifocuser.h>
#include <memory>
#include "astralim_gpio.h"

class AstrAlimFocuser : public INDI::Focuser
{
public:
    AstrAlimFocuser();
    virtual ~AstrAlimFocuser() = default;

    const char* getDefaultName() override;
    
    bool initProperties() override;
    bool updateProperties() override;
    
    bool ISNewNumber(const char* dev, const char* name, double values[], char* names[], int n) override;
    bool ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n) override;
    bool ISNewText(const char* dev, const char* name, char* texts[], char* names[], int n) override;
    bool ISSnoopDevice(XMLEle* root) override;

protected:
    bool Connect() override;
    bool Disconnect() override;
    
    IPState MoveAbsFocuser(uint32_t targetTicks) override;
    IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    bool ReverseFocuser(bool enabled) override;
    bool SyncFocuser(uint32_t ticks) override;
    bool SetFocuserBacklash(int32_t steps) override;
    bool AbortFocuser() override;
    void TimerHit() override;
    bool saveConfigItems(FILE* fp) override;

private:
    // Motor control
    void stepMotor();
    void setResolution(int res);
    void wakeUpMotor();
    void sleepMotor();
    
    // Position persistence
    int savePosition(int pos);
    int loadPosition();
    
    // Temperature
    bool readDS18B20();
    void updateTemperature();
    void temperatureCompensation();
    
    // Info calculation
    void updateFocuserInfo();
    
    // Timer callbacks
    static void stepperStandbyHelper(void* context);
    static void updateTemperatureHelper(void* context);
    static void temperatureCompensationHelper(void* context);
    void stepperStandby();

    // GPIO controller
    std::unique_ptr<AstrAlim::GpioController> gpio;

    // Properties - Resolution
    INDI::PropertySwitch FocusResolutionSP {6};
    enum { RES_1, RES_2, RES_4, RES_8, RES_16, RES_32 };
    
    // Properties - Motor Board
    INDI::PropertySwitch MotorBoardSP {2};
    enum { BOARD_DRV8834, BOARD_A4988 };
    
    // Properties - Temperature Compensation
    INDI::PropertySwitch TemperatureCompensateSP {2};
    enum { TC_ENABLED, TC_DISABLED };
    
    // Properties - Stepper Standby
    INDI::PropertySwitch StepperStandbySP {2};
    enum { STANDBY_ENABLED, STANDBY_DISABLED };
    
    // Properties - Numbers
    INDI::PropertyNumber FocuserInfoNP {3};
    INDI::PropertyNumber StepperStandbyTimeNP {1};
    INDI::PropertyNumber FocusStepDelayNP {1};
    INDI::PropertyNumber FocuserTravelNP {1};
    INDI::PropertyNumber FocusTemperatureNP {1};
    INDI::PropertyNumber TemperatureCoefNP {1};
    INDI::PropertyNumber ScopeParametersNP {2};
    
    // Properties - Text
    INDI::PropertyText ActiveTelescopeTP {1};

    // State variables
    int resolution = 1;
    int stepperDirection = 1;
    int backlashTicksRemaining = 0;
    int focuserTicksRemaining = 0;
    float lastTemperature = 0;
    bool motorAwake = false;
    
    // Timer IDs
    int stepperStandbyID = -1;
    int updateTemperatureID = -1;
    int temperatureCompensationID = -1;
    
    // Constants
    static constexpr int MAX_RESOLUTION = 32;
    static constexpr int MINMAX_MIN_POS = 0;
    static constexpr int MINMAX_MAX_POS = 100000;
    static constexpr int TEMPERATURE_UPDATE_TIMEOUT = 60000;
    static constexpr int TEMPERATURE_COMPENSATION_TIMEOUT = 60000;
};

#endif // ASTRALIM_FOCUSER_H

