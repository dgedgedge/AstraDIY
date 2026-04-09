/*******************************************************************************
 * AstrAlim System Driver - Modern INDI API
 * Copyright (c) 2024 AstrAlim Project
 * Based on original work by Radek Kaczorek
 ******************************************************************************/

#ifndef ASTRALIM_SYSTEM_H
#define ASTRALIM_SYSTEM_H

#include <defaultdevice.h>
#include <deque>
#include <string>

class AstrAlimSystem : public INDI::DefaultDevice
{
public:
    AstrAlimSystem();
    virtual ~AstrAlimSystem() = default;

    const char* getDefaultName() override;
    
    bool initProperties() override;
    bool updateProperties() override;
    
    bool ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n) override;

protected:
    bool Connect() override;
    bool Disconnect() override;
    void TimerHit() override;

private:
    void updateSystemInfo();
    void updateTime();
    void updateDiskSpace();
    void updateNtpInfo();
    std::string execCommand(const char* cmd);
    std::string formatDiskSpace(const std::string& device, const std::string& mountPoint, 
                                 const std::string& size, const std::string& used, const std::string& avail, 
                                 const std::string& percent);

    // Properties - System Time
    INDI::PropertyText SysTimeTP {2};
    
    // Properties - System Info
    INDI::PropertyText SysInfoTP {6};
    
    // Properties - Disk Space (root + up to 4 USB drives)
    INDI::PropertyText DiskSpaceTP {5};

    // Properties - NTP metrics (aligned with AstraGps Python calculations)
    INDI::PropertyText NtpInfoTP {6};
    
    // Properties - System Control
    INDI::PropertySwitch SysControlSP {2};
    enum { CTRL_REBOOT, CTRL_SHUTDOWN };
    
    // Properties - Confirmation
    INDI::PropertySwitch SysConfirmSP {2};
    enum { CONFIRM_YES, CONFIRM_NO };

    int pollCounter = 0;
    static constexpr int POLL_INTERVAL_MS = 1000;
    static constexpr int INFO_UPDATE_CYCLES = 60;

    // NTP rolling samples (same spirit as Python module)
    std::deque<double> ntpOffsetsS;
    std::deque<double> ntpDelaysS;
    std::deque<double> ntpRootDispersionS;
    static constexpr size_t NTP_MAX_SAMPLES = 20;
};

#endif // ASTRALIM_SYSTEM_H

