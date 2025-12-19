/*******************************************************************************
 * AstrAlim System Driver - Modern INDI API Implementation
 * Copyright (c) 2024 AstrAlim Project
 * Based on original work by Radek Kaczorek
 ******************************************************************************/

#include "astralim_system.h"
#include "config.h"

#include <cstring>
#include <ctime>
#include <array>
#include <memory>
#include <sstream>
#include <vector>

// Singleton instance
static std::unique_ptr<AstrAlimSystem> systemInstance(new AstrAlimSystem());

AstrAlimSystem::AstrAlimSystem()
{
    setVersion(INDI_ASTRALIM_VERSION_MAJOR, INDI_ASTRALIM_VERSION_MINOR);
}

const char* AstrAlimSystem::getDefaultName()
{
    return "AstrAlim System";
}

bool AstrAlimSystem::initProperties()
{
    INDI::DefaultDevice::initProperties();
    
    // System Time
    SysTimeTP[0].fill("LOCAL_TIME", "Local Time", nullptr);
    SysTimeTP[1].fill("UTC_OFFSET", "UTC Offset", nullptr);
    SysTimeTP.fill(getDeviceName(), "SYSTEM_TIME", "System Time", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // System Info
    SysInfoTP[0].fill("HARDWARE", "Hardware", nullptr);
    SysInfoTP[1].fill("CPU_TEMP", "CPU Temp (°C)", nullptr);
    SysInfoTP[2].fill("UPTIME", "Uptime", nullptr);
    SysInfoTP[3].fill("LOAD", "Load (1/5/15 min)", nullptr);
    SysInfoTP[4].fill("HOSTNAME", "Hostname", nullptr);
    SysInfoTP[5].fill("LOCAL_IP", "Local IP", nullptr);
    SysInfoTP.fill(getDeviceName(), "SYSTEM_INFO", "System Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Disk Space (root + USB drives)
    DiskSpaceTP[0].fill("ROOT_DISK", "Disque système", nullptr);
    DiskSpaceTP[1].fill("USB1", "USB 1", nullptr);
    DiskSpaceTP[2].fill("USB2", "USB 2", nullptr);
    DiskSpaceTP[3].fill("USB3", "USB 3", nullptr);
    DiskSpaceTP[4].fill("USB4", "USB 4", nullptr);
    DiskSpaceTP.fill(getDeviceName(), "DISK_SPACE", "Espace disque", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // System Control
    SysControlSP[CTRL_REBOOT].fill("REBOOT", "Reboot", ISS_OFF);
    SysControlSP[CTRL_SHUTDOWN].fill("SHUTDOWN", "Shutdown", ISS_OFF);
    SysControlSP.fill(getDeviceName(), "SYSTEM_CONTROL", "System Control", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Confirmation dialog
    SysConfirmSP[CONFIRM_YES].fill("CONFIRM_YES", "Yes", ISS_OFF);
    SysConfirmSP[CONFIRM_NO].fill("CONFIRM_NO", "No", ISS_OFF);
    SysConfirmSP.fill(getDeviceName(), "CONFIRM_ACTION", "Confirm?", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    addDebugControl();
    setDefaultPollingPeriod(POLL_INTERVAL_MS);
    
    return true;
}

bool AstrAlimSystem::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    
    if (isConnected())
    {
        defineProperty(SysTimeTP);
        defineProperty(SysInfoTP);
        defineProperty(DiskSpaceTP);
        defineProperty(SysControlSP);
    }
    else
    {
        deleteProperty(SysTimeTP);
        deleteProperty(SysInfoTP);
        deleteProperty(DiskSpaceTP);
        deleteProperty(SysControlSP);
        deleteProperty(SysConfirmSP);
    }
    
    return true;
}

bool AstrAlimSystem::Connect()
{
    // Get initial system info
    updateSystemInfo();
    updateDiskSpace();
    
    // Start timer
    SetTimer(POLL_INTERVAL_MS);
    
    LOG_INFO("AstrAlim System connected");
    return true;
}

bool AstrAlimSystem::Disconnect()
{
    LOG_INFO("AstrAlim System disconnected");
    return true;
}

void AstrAlimSystem::TimerHit()
{
    if (!isConnected())
        return;
    
    // Update time every tick
    updateTime();
    
    // Update system info less frequently
    if (++pollCounter >= INFO_UPDATE_CYCLES)
    {
        updateSystemInfo();
        updateDiskSpace();
        pollCounter = 0;
    }
    
    SetTimer(POLL_INTERVAL_MS);
}

void AstrAlimSystem::updateTime()
{
    time_t rawtime;
    time(&rawtime);
    struct tm* local_time = localtime(&rawtime);
    
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", local_time);
    SysTimeTP[0].setText(timeStr);
    
    char offsetStr[16];
    snprintf(offsetStr, sizeof(offsetStr), "%+.2f", local_time->tm_gmtoff / 3600.0);
    SysTimeTP[1].setText(offsetStr);
    
    SysTimeTP.setState(IPS_OK);
    SysTimeTP.apply();
}

void AstrAlimSystem::updateSystemInfo()
{
    SysInfoTP.setState(IPS_BUSY);
    SysInfoTP.apply();
    
    // Hardware model
    std::string hw = execCommand("cat /sys/firmware/devicetree/base/model 2>/dev/null");
    if (!hw.empty()) hw.pop_back(); // Remove trailing newline
    SysInfoTP[0].setText(hw.c_str());
    
    // CPU temperature
    std::string temp = execCommand("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
    if (!temp.empty())
    {
        int tempMilliC = std::stoi(temp);
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%d", tempMilliC / 1000);
        SysInfoTP[1].setText(tempStr);
    }
    
    // Uptime
    std::string uptime = execCommand("uptime -p 2>/dev/null | sed 's/up //'");
    if (!uptime.empty()) uptime.pop_back();
    SysInfoTP[2].setText(uptime.c_str());
    
    // Load average
    std::string load = execCommand("cat /proc/loadavg 2>/dev/null | awk '{print $1\" / \"$2\" / \"$3}'");
    if (!load.empty()) load.pop_back();
    SysInfoTP[3].setText(load.c_str());
    
    // Hostname
    std::string hostname = execCommand("hostname 2>/dev/null");
    if (!hostname.empty()) hostname.pop_back();
    SysInfoTP[4].setText(hostname.c_str());
    
    // Local IP
    std::string ip = execCommand("hostname -I 2>/dev/null | awk '{print $1}'");
    if (!ip.empty()) ip.pop_back();
    SysInfoTP[5].setText(ip.c_str());
    
    SysInfoTP.setState(IPS_OK);
    SysInfoTP.apply();
}

void AstrAlimSystem::updateDiskSpace()
{
    DiskSpaceTP.setState(IPS_BUSY);
    DiskSpaceTP.apply();
    
    // Reset all disk space fields
    for (int i = 0; i < 5; i++)
    {
        DiskSpaceTP[i].setText("");
    }
    
    // Get root filesystem info
    std::string rootCmd = "df -h / 2>/dev/null | tail -1 | awk '{print $1\"|\"$2\"|\"$3\"|\"$4\"|\"$5\"|\"$6}'";
    std::string rootLine = execCommand(rootCmd.c_str());
    if (!rootLine.empty() && rootLine.back() == '\n') rootLine.pop_back();
    
    int usbIndex = 1; // Start at USB1 (index 1, index 0 is root)
    
    // Parse root filesystem
    if (!rootLine.empty())
    {
        std::istringstream rootStream(rootLine);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(rootStream, token, '|'))
        {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 6)
        {
            std::string device = tokens[0];
            std::string size = tokens[1];
            std::string used = tokens[2];
            std::string avail = tokens[3];
            std::string percent = tokens[4];
            std::string mountPoint = tokens[5];
            
            if (mountPoint == "/")
            {
                std::string formatted = formatDiskSpace(device, mountPoint, size, used, avail, percent);
                DiskSpaceTP[0].setText(formatted.c_str());
            }
        }
    }
    
    // Get USB drives (mounted in /media/ or /mnt/)
    std::string usbCmd = "df -h 2>/dev/null | grep -E '^/dev/' | grep -E '/media/|/mnt/' | grep -v '/dev/loop' | awk '{print $1\"|\"$2\"|\"$3\"|\"$4\"|\"$5\"|\"$6}'";
    std::string usbOutput = execCommand(usbCmd.c_str());
    
    if (!usbOutput.empty())
    {
        std::istringstream usbStream(usbOutput);
        std::string line;
        
        while (std::getline(usbStream, line) && usbIndex < 5)
        {
            if (line.empty()) continue;
            if (line.back() == '\n') line.pop_back();
            
            std::istringstream lineStream(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (std::getline(lineStream, token, '|'))
            {
                tokens.push_back(token);
            }
            
            if (tokens.size() >= 6)
            {
                std::string device = tokens[0];
                std::string size = tokens[1];
                std::string used = tokens[2];
                std::string avail = tokens[3];
                std::string percent = tokens[4];
                std::string mountPoint = tokens[5];
                
                // Extract USB label from mount point (e.g., /media/user/USBKEY -> USBKEY)
                std::string usbLabel = mountPoint;
                size_t lastSlash = usbLabel.find_last_of('/');
                if (lastSlash != std::string::npos && lastSlash < usbLabel.length() - 1)
                {
                    usbLabel = usbLabel.substr(lastSlash + 1);
                }
                else
                {
                    usbLabel = mountPoint; // Fallback to full path if no slash found
                }
                
                std::string formatted = formatDiskSpace(device, usbLabel, size, used, avail, percent);
                DiskSpaceTP[usbIndex].setText(formatted.c_str());
                usbIndex++;
            }
        }
    }
    
    // Set empty USB slots to "Non monté"
    for (int i = usbIndex; i < 5; i++)
    {
        if (DiskSpaceTP[i].getText() == nullptr || strlen(DiskSpaceTP[i].getText()) == 0)
        {
            DiskSpaceTP[i].setText("Non monté");
        }
    }
    
    DiskSpaceTP.setState(IPS_OK);
    DiskSpaceTP.apply();
}

std::string AstrAlimSystem::formatDiskSpace(const std::string& device, const std::string& mountPoint, 
                                             const std::string& size, const std::string& used, 
                                             const std::string& avail, const std::string& percent)
{
    // For root filesystem: "250GO / 500GO - 50% libre"
    // For USB: "USBKEY: 2.1G libres / 8.0G total (26% utilisé) - /dev/sdb1"
    
    if (mountPoint == "/")
    {
        // Format simplifié pour le disque système
        std::string sizeGO = size;
        std::string usedGO = used;
        
        // Convertir les unités en "GO" si nécessaire (G -> GO, M -> MO, etc.)
        if (sizeGO.back() == 'G')
        {
            sizeGO += "O";
        }
        else if (sizeGO.back() == 'M')
        {
            sizeGO += "O";
        }
        else if (sizeGO.back() == 'K')
        {
            sizeGO += "O";
        }
        else if (sizeGO.back() == 'T')
        {
            sizeGO += "O";
        }
        
        if (usedGO.back() == 'G')
        {
            usedGO += "O";
        }
        else if (usedGO.back() == 'M')
        {
            usedGO += "O";
        }
        else if (usedGO.back() == 'K')
        {
            usedGO += "O";
        }
        else if (usedGO.back() == 'T')
        {
            usedGO += "O";
        }
        
        // Calculer le pourcentage libre
        std::string percentClean = percent;
        if (!percentClean.empty() && percentClean.back() == '%')
        {
            percentClean.pop_back();
        }
        
        int percentUsed = 0;
        try
        {
            percentUsed = std::stoi(percentClean);
        }
        catch (...)
        {
            percentUsed = 0;
        }
        
        int percentFree = 100 - percentUsed;
        
        char formatted[128];
        snprintf(formatted, sizeof(formatted), "%s / %s - %d%% libre", 
                 usedGO.c_str(), sizeGO.c_str(), percentFree);
        
        return std::string(formatted);
    }
    else
    {
        // Format détaillé pour les USB
        std::string label = mountPoint;
        std::string percentClean = percent;
        if (!percentClean.empty() && percentClean.back() == '%')
        {
            percentClean.pop_back();
        }
        
        char formatted[128];
        snprintf(formatted, sizeof(formatted), "%s: %s libres / %s total (%s%% utilisé) - %s", 
                 label.c_str(), avail.c_str(), size.c_str(), percentClean.c_str(), device.c_str());
        
        return std::string(formatted);
    }
}

std::string AstrAlimSystem::execCommand(const char* cmd)
{
    std::array<char, 256> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
    {
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    
    return result;
}

bool AstrAlimSystem::ISNewSwitch(const char* dev, const char* name, ISState* states, char* names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        // System control
        if (SysControlSP.isNameMatch(name))
        {
            SysControlSP.update(states, names, n);
            SysControlSP.setState(IPS_BUSY);
            SysControlSP.apply();
            
            if (SysControlSP[CTRL_REBOOT].getState() == ISS_ON)
            {
                LOG_WARN("System REBOOT requested. Confirm to proceed.");
            }
            else if (SysControlSP[CTRL_SHUTDOWN].getState() == ISS_ON)
            {
                LOG_WARN("System SHUTDOWN requested. Confirm to proceed.");
            }
            
            // Show confirmation dialog
            defineProperty(SysConfirmSP);
            return true;
        }
        
        // Confirmation
        if (SysConfirmSP.isNameMatch(name))
        {
            SysConfirmSP.update(states, names, n);
            
            if (SysConfirmSP[CONFIRM_YES].getState() == ISS_ON)
            {
                if (SysControlSP[CTRL_REBOOT].getState() == ISS_ON)
                {
                    LOG_WARN("Rebooting system...");
                    execCommand("sudo reboot");
                }
                else if (SysControlSP[CTRL_SHUTDOWN].getState() == ISS_ON)
                {
                    LOG_WARN("Shutting down system...");
                    execCommand("sudo poweroff");
                }
            }
            else
            {
                LOG_INFO("Operation cancelled");
            }
            
            // Reset controls
            SysControlSP.reset();
            SysControlSP.setState(IPS_IDLE);
            SysControlSP.apply();
            
            SysConfirmSP.reset();
            deleteProperty(SysConfirmSP);
            
            return true;
        }
    }
    
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

