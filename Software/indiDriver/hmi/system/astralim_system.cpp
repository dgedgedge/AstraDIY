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
#include <chrono>
#include <memory>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <cstdint>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace
{
constexpr const char* SYSTEM_TIME_TAB = "Time & NTP";
constexpr const char* SYSTEM_INFO_TAB = "System Info";
constexpr const char* SYSTEM_STORAGE_TAB = "Storage";
constexpr const char* SYSTEM_ACTIONS_TAB = "Actions";

double meanValue(const std::deque<double>& samples)
{
    if (samples.empty())
        return 0.0;

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    return sum / static_cast<double>(samples.size());
}

double sampleStdDev(const std::deque<double>& samples)
{
    if (samples.size() <= 1)
        return 0.0;

    const double mean = meanValue(samples);
    double sqSum = 0.0;
    for (const double value : samples)
    {
        const double delta = value - mean;
        sqSum += delta * delta;
    }

    return std::sqrt(sqSum / static_cast<double>(samples.size() - 1));
}

constexpr double NTP_UNIX_EPOCH_DELTA_S = 2208988800.0;

double nowUnixSeconds()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

uint32_t readBe32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

double ntpTimestampToUnixSeconds(const uint8_t* ts)
{
    const uint32_t sec = readBe32(ts);
    const uint32_t frac = readBe32(ts + 4);
    const double fracS = static_cast<double>(frac) / 4294967296.0;
    return static_cast<double>(sec) - NTP_UNIX_EPOCH_DELTA_S + fracS;
}

void writeUnixSecondsAsNtpTimestamp(double unixS, uint8_t* ts)
{
    const double ntpS = unixS + NTP_UNIX_EPOCH_DELTA_S;
    const uint32_t sec = static_cast<uint32_t>(std::floor(ntpS));
    const double frac = ntpS - std::floor(ntpS);
    const uint32_t fracPart = static_cast<uint32_t>(frac * 4294967296.0);

    ts[0] = static_cast<uint8_t>((sec >> 24) & 0xFF);
    ts[1] = static_cast<uint8_t>((sec >> 16) & 0xFF);
    ts[2] = static_cast<uint8_t>((sec >> 8) & 0xFF);
    ts[3] = static_cast<uint8_t>(sec & 0xFF);
    ts[4] = static_cast<uint8_t>((fracPart >> 24) & 0xFF);
    ts[5] = static_cast<uint8_t>((fracPart >> 16) & 0xFF);
    ts[6] = static_cast<uint8_t>((fracPart >> 8) & 0xFF);
    ts[7] = static_cast<uint8_t>(fracPart & 0xFF);
}

std::string decodeRefSource(const uint8_t* refIdBytes, int stratum)
{
    if (stratum <= 1)
    {
        char code[5] = {};
        for (int i = 0; i < 4; ++i)
            code[i] = std::isprint(refIdBytes[i]) ? static_cast<char>(refIdBytes[i]) : '.';
        return std::string(code);
    }

    char ip[16] = {};
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u",
             static_cast<unsigned int>(refIdBytes[0]),
             static_cast<unsigned int>(refIdBytes[1]),
             static_cast<unsigned int>(refIdBytes[2]),
             static_cast<unsigned int>(refIdBytes[3]));
    return std::string(ip);
}

} // namespace

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
    SysTimeTP[0].fill("LOCAL_TIME", "Date | Heure | UTC", nullptr);
    SysTimeTP.fill(getDeviceName(), "SYSTEM_TIME", "System Time", SYSTEM_TIME_TAB, IP_RO, 60, IPS_IDLE);

    // NTP metrics
    NtpInfoTP[0].fill("NTP_TIME", "Heure NTP (UTC)", nullptr);
    NtpInfoTP[1].fill("NTP_STRATUM", "Stratum", nullptr);
    NtpInfoTP[2].fill("NTP_REF_SOURCE", "Ref Source", nullptr);
    NtpInfoTP[3].fill("NTP_PRECISION_US", "Precision (us)", nullptr);
    NtpInfoTP[4].fill("NTP_OFFSET_US", "Decalage (us)", nullptr);
    NtpInfoTP[5].fill("NTP_DELAY_MS", "Delai (ms)", nullptr);
    NtpInfoTP[6].fill("NTP_ROOT_DISPERSION_MS", "Root Dispersion (ms)", nullptr);
    NtpInfoTP[7].fill("NTP_DISPERSION_MS", "Dispersion (ms)", nullptr);
    NtpInfoTP[8].fill("NTP_JITTER_MS", "Jitter (ms)", nullptr);
    NtpInfoTP.fill(getDeviceName(), "NTP_INFO", "NTP", SYSTEM_TIME_TAB, IP_RO, 60, IPS_IDLE);
    
    // System Info
    SysInfoTP[0].fill("HARDWARE", "Hardware", nullptr);
    SysInfoTP[1].fill("CPU_TEMP", "CPU Temp (°C)", nullptr);
    SysInfoTP[2].fill("UPTIME", "Uptime", nullptr);
    SysInfoTP[3].fill("LOAD", "Load (1/5/15 min)", nullptr);
    SysInfoTP[4].fill("HOSTNAME", "Hostname", nullptr);
    SysInfoTP[5].fill("LOCAL_IP", "Local IP", nullptr);
    SysInfoTP.fill(getDeviceName(), "SYSTEM_INFO", "System Info", SYSTEM_INFO_TAB, IP_RO, 60, IPS_IDLE);
    
    // Disk Space (root + USB drives)
    DiskSpaceTP[0].fill("ROOT_DISK", "Disque système", nullptr);
    DiskSpaceTP[1].fill("USB1", "USB 1", nullptr);
    DiskSpaceTP[2].fill("USB2", "USB 2", nullptr);
    DiskSpaceTP[3].fill("USB3", "USB 3", nullptr);
    DiskSpaceTP[4].fill("USB4", "USB 4", nullptr);
    DiskSpaceTP.fill(getDeviceName(), "DISK_SPACE", "Espace disque", SYSTEM_STORAGE_TAB, IP_RO, 60, IPS_IDLE);

    // System Control
    SysControlSP[CTRL_REBOOT].fill("REBOOT", "Reboot", ISS_OFF);
    SysControlSP[CTRL_SHUTDOWN].fill("SHUTDOWN", "Shutdown", ISS_OFF);
    SysControlSP.fill(getDeviceName(), "SYSTEM_CONTROL", "System Control", SYSTEM_ACTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Confirmation dialog
    SysConfirmSP[CONFIRM_YES].fill("CONFIRM_YES", "Yes", ISS_OFF);
    SysConfirmSP[CONFIRM_NO].fill("CONFIRM_NO", "No", ISS_OFF);
    SysConfirmSP.fill(getDeviceName(), "CONFIRM_ACTION", "Confirm?", SYSTEM_ACTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
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
        defineProperty(NtpInfoTP);
        defineProperty(SysInfoTP);
        defineProperty(DiskSpaceTP);
        defineProperty(SysControlSP);
    }
    else
    {
        deleteProperty(SysTimeTP);
        deleteProperty(NtpInfoTP);
        deleteProperty(SysInfoTP);
        deleteProperty(DiskSpaceTP);
        deleteProperty(SysControlSP);
        deleteProperty(SysConfirmSP);
    }
    
    return true;
}

bool AstrAlimSystem::Connect()
{
    updateTime();

    // Get initial system info
    updateSystemInfo();
    updateDiskSpace();
    updateNtpInfo();
    
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
    updateNtpInfo();
    
    // Update system info less frequently
    if (++pollCounter >= INFO_UPDATE_CYCLES)
    {
        updateSystemInfo();
        updateDiskSpace();
        pollCounter = 0;
    }
    
    SetTimer(POLL_INTERVAL_MS);
}

void AstrAlimSystem::updateNtpInfo()
{
    double txTimeUnixS = 0.0;
    double offsetS = 0.0;
    double delayS = 0.0;
    double rootDispersionS = 0.0;
    int stratum = 0;
    std::string refSource;

    if (!queryNtpSample(txTimeUnixS, offsetS, delayS, rootDispersionS, stratum, refSource))
    {
        NtpInfoTP[0].setText("--:--:--");
        NtpInfoTP[1].setText("--");
        NtpInfoTP[2].setText("--");
        NtpInfoTP[3].setText("--");
        NtpInfoTP[4].setText("--");
        NtpInfoTP[5].setText("--");
        NtpInfoTP[6].setText("--");
        NtpInfoTP[7].setText("--");
        NtpInfoTP[8].setText("--");
        NtpInfoTP.setState(IPS_IDLE);
        NtpInfoTP.apply();
        return;
    }

    // Stratum 0 = KoD (non synchronisé), stratum >= 16 = horloge invalide
    const bool synchronized = (stratum >= 1 && stratum < 16);

    char stratumStr[32];
    if (stratum == 0)
        snprintf(stratumStr, sizeof(stratumStr), "0 (non sync.)");
    else if (stratum >= 16)
        snprintf(stratumStr, sizeof(stratumStr), "%d (invalide)", stratum);
    else
        snprintf(stratumStr, sizeof(stratumStr), "%d", stratum);

    char hms[16];
    const time_t txTime = static_cast<time_t>(txTimeUnixS);
    std::tm utcTm {};
    gmtime_r(&txTime, &utcTm);
    strftime(hms, sizeof(hms), "%H:%M:%S", &utcTm);

    if (!synchronized)
    {
        // Vider les échantillons accumulés pour éviter des valeurs périmées
        ntpOffsetsS.clear();
        ntpDelaysS.clear();
        ntpRootDispersionS.clear();

        NtpInfoTP[0].setText(hms);
        NtpInfoTP[1].setText(stratumStr);
        NtpInfoTP[2].setText(refSource.c_str());
        NtpInfoTP[3].setText("N/A");
        NtpInfoTP[4].setText("N/A");
        NtpInfoTP[5].setText("N/A");
        NtpInfoTP[6].setText("N/A");
        NtpInfoTP[7].setText("N/A");
        NtpInfoTP[8].setText("N/A");
        NtpInfoTP.setState(IPS_ALERT);
        NtpInfoTP.apply();
        return;
    }

    ntpOffsetsS.push_back(offsetS);
    if (ntpOffsetsS.size() > NTP_MAX_SAMPLES)
        ntpOffsetsS.pop_front();

    ntpDelaysS.push_back(delayS);
    if (ntpDelaysS.size() > NTP_MAX_SAMPLES)
        ntpDelaysS.pop_front();

    ntpRootDispersionS.push_back(rootDispersionS);
    if (ntpRootDispersionS.size() > NTP_MAX_SAMPLES)
        ntpRootDispersionS.pop_front();

    const double meanOffsetS = std::fabs(meanValue(ntpOffsetsS));
    const double dispersionS = sampleStdDev(ntpDelaysS);
    const double jitterS = sampleStdDev(ntpOffsetsS);
    const double delayMeanS = meanValue(ntpDelaysS);
    const double rootDispersionMeanS = meanValue(ntpRootDispersionS);
    const double uncertaintyS = meanOffsetS + dispersionS + jitterS;

    char precisionUsStr[32];
    char offsetUsStr[32];
    char delayMsStr[32];
    char rootDispMsStr[32];
    char dispersionMsStr[32];
    char jitterMsStr[32];
    snprintf(precisionUsStr, sizeof(precisionUsStr), "%.1f", uncertaintyS * 1e6);
    snprintf(offsetUsStr, sizeof(offsetUsStr), "%.1f", meanOffsetS * 1e6);
    snprintf(delayMsStr, sizeof(delayMsStr), "%.3f", delayMeanS * 1e3);
    snprintf(rootDispMsStr, sizeof(rootDispMsStr), "%.3f", rootDispersionMeanS * 1e3);
    snprintf(dispersionMsStr, sizeof(dispersionMsStr), "%.3f", dispersionS * 1e3);
    snprintf(jitterMsStr, sizeof(jitterMsStr), "%.3f", jitterS * 1e3);

    NtpInfoTP[0].setText(hms);
    NtpInfoTP[1].setText(stratumStr);
    NtpInfoTP[2].setText(refSource.c_str());
    NtpInfoTP[3].setText(precisionUsStr);
    NtpInfoTP[4].setText(offsetUsStr);
    NtpInfoTP[5].setText(delayMsStr);
    NtpInfoTP[6].setText(rootDispMsStr);
    NtpInfoTP[7].setText(dispersionMsStr);
    NtpInfoTP[8].setText(jitterMsStr);
    NtpInfoTP.setState(IPS_OK);
    NtpInfoTP.apply();
}

bool AstrAlimSystem::queryNtpSample(double& txTimeUnixS, double& offsetS, double& delayS, double& rootDispersionS,
                                    int& stratum, std::string& refSource)
{
    // NTP request/response packet (RFC 5905), 48 bytes.
    uint8_t packet[48] = {0};
    packet[0] = 0x23; // LI=0, VN=4, Mode=3 (client)

    const double t1 = nowUnixSeconds();
    writeUnixSecondsAsNtpTimestamp(t1, &packet[40]);

    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
        return false;

    timeval timeout {};
    timeout.tv_sec = NTP_TIMEOUT_MS / 1000;
    timeout.tv_usec = (NTP_TIMEOUT_MS % 1000) * 1000;
    (void) setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(123);
    if (inet_pton(AF_INET, NTP_SERVER, &addr.sin_addr) != 1)
    {
        ::close(sock);
        return false;
    }

    const ssize_t sent = sendto(sock, packet, sizeof(packet), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (sent != static_cast<ssize_t>(sizeof(packet)))
    {
        ::close(sock);
        return false;
    }

    sockaddr_in srcAddr {};
    socklen_t srcLen = sizeof(srcAddr);
    const ssize_t received = recvfrom(sock, packet, sizeof(packet), 0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
    const double t4 = nowUnixSeconds();
    ::close(sock);

    if (received < 48)
        return false;

    const double t2 = ntpTimestampToUnixSeconds(&packet[32]);
    const double t3 = ntpTimestampToUnixSeconds(&packet[40]);

    txTimeUnixS = t3;
    offsetS = std::fabs(((t2 - t1) + (t3 - t4)) / 2.0);
    delayS = std::fabs((t4 - t1) - (t3 - t2));

    // Root dispersion is an unsigned 16.16 fixed-point field.
    const uint32_t rootDispRaw = readBe32(&packet[8]);
    rootDispersionS = static_cast<double>(rootDispRaw) / 65536.0;

    stratum = static_cast<int>(packet[1]);
    refSource = decodeRefSource(&packet[12], stratum);

    return true;
}

void AstrAlimSystem::updateTime()
{
    time_t rawtime;
    time(&rawtime);
    struct tm* local_time = localtime(&rawtime);
    
    char dateStr[16];
    char hourStr[16];
    char offsetStr[16];
    char localTimeStr[64];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", local_time);
    strftime(hourStr, sizeof(hourStr), "%H:%M:%S", local_time);
    snprintf(offsetStr, sizeof(offsetStr), "%+.2f", local_time->tm_gmtoff / 3600.0);

    snprintf(localTimeStr, sizeof(localTimeStr), "%s | %s | UTC%s", dateStr, hourStr, offsetStr);
    SysTimeTP[0].setText(localTimeStr);
    
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
    struct PipeCloser
    {
        void operator()(FILE* file) const noexcept
        {
            if (file != nullptr)
                pclose(file);
        }
    };

    std::array<char, 256> buffer;
    std::string result;

    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd, "r"));
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

