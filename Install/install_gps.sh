#!/bin/bash
set -euo pipefail

FILESOURCE=$(readlink -f $(dirname $0))
BOOTFILE="/boot/firmware/config.txt"
CMDLINEFILE="/boot/firmware/cmdline.txt"
GPSD_DEFAULT_SRC="${FILESOURCE}/gps/etc_default_gpsd"
CHRONY_CONF_SRC="${FILESOURCE}/gps/etc_chrony_conf.d_chronyastralim.conf"

echo "Begin Installing GPS"

requireRoot() {
    if [[ "${EUID}" -ne 0 ]]; then
        echo "This script must be run as root."
        exit 1
    fi
}

backupFile() {
    local filePath="$1"
    if [[ -f "${filePath}" ]]; then
        local backupPath="${filePath}.bak.$(date +%Y%m%d-%H%M%S)"
        cp -a "${filePath}" "${backupPath}"
        echo "Backup created: ${backupPath}"
    fi
}

getPiModel() {
    if [[ -r /proc/device-tree/model ]]; then
        tr -d '\0' < /proc/device-tree/model
    else
        echo "Unknown Raspberry Pi"
    fi
}

isPi5() {
    local model
    model="$(getPiModel)"
    [[ "${model}" == *"Raspberry Pi 5"* ]]
}

ensureLineInFile() {
    local filePath="$1"
    local exactLine="$2"

    if ! grep -Fqx "${exactLine}" "${filePath}" 2>/dev/null; then
        echo "${exactLine}" >> "${filePath}"
        return 0
    fi
    return 1
}

ensureBlockMarker() {
    local filePath="$1"
    local marker="$2"

    if ! grep -Fq "${marker}" "${filePath}" 2>/dev/null; then
        echo "${marker}" >> "${filePath}"
        return 0
    fi
    return 1
}

removeSerialConsoleFromCmdline() {
    local filePath="$1"

    if [[ ! -f "${filePath}" ]]; then
        echo "Warning: ${filePath} not found."
        return 1
    fi

    local originalLine
    originalLine="$(cat "${filePath}")"

    if [[ "${originalLine}" =~ (^|[[:space:]])console=serial0,[^[:space:]]+($|[[:space:]]) ]]; then
        local newLine
        newLine="$(printf '%s\n' "${originalLine}" \
            | sed -E 's/(^|[[:space:]])console=serial0,[^[:space:]]+([[:space:]]|$)/ /g' \
            | tr -s ' ')"
        newLine="${newLine#" "}"
        newLine="${newLine%" "}"

        printf '%s\n' "${newLine}" > "${filePath}"
        echo "Removed console=serial0,... from ${filePath}"
        return 0
    fi

    echo "No console=serial0,... found in ${filePath}"
    return 1
}

disableSerialGetty() {
    if systemctl list-unit-files | grep -q '^serial-getty@serial0\.service'; then
        systemctl stop serial-getty@serial0.service || true
        systemctl disable serial-getty@serial0.service || true
        echo "Disabled serial-getty@serial0.service"
    else
        echo "serial-getty@serial0.service not present"
    fi
}

installGpsPackagesAndConfigs() {
    apt-get update
    apt-get install -y gpsd chrony pps-tools

    if [[ -f "${GPSD_DEFAULT_SRC}" ]]; then
        install -m 644 "${GPSD_DEFAULT_SRC}" /etc/default/gpsd
    else
        echo "Warning: ${GPSD_DEFAULT_SRC} not found"
    fi

    if [[ -f "${CHRONY_CONF_SRC}" ]]; then
        install -m 644 "${CHRONY_CONF_SRC}" /etc/chrony/conf.d/chronyastralim.conf
    else
        echo "Warning: ${CHRONY_CONF_SRC} not found"
    fi
}

updateBootConfig() {
    local changed=0

    ensureBlockMarker "${BOOTFILE}" "" && changed=1 || true
    ensureBlockMarker "${BOOTFILE}" "# Begin AstrAlim GPS" && changed=1 || true

    ensureLineInFile "${BOOTFILE}" "dtoverlay=pps-gpio,gpiopin=25" && changed=1 || true

    if isPi5; then
        echo "Detected Raspberry Pi 5"
        ensureLineInFile "${BOOTFILE}" "dtoverlay=uart0" && changed=1 || true
    else
        echo "Detected non-Pi5 Raspberry Pi"
        ensureLineInFile "${BOOTFILE}" "enable_uart=1" && changed=1 || true
        ensureLineInFile "${BOOTFILE}" "dtoverlay=uart0" && changed=1 || true
    fi

    ensureBlockMarker "${BOOTFILE}" "# End AstrAlim GPS" && changed=1 || true

    return "${changed}"
}

main() {
    requireRoot

    echo "Model: $(getPiModel)"

    backupFile "${BOOTFILE}"
    backupFile "${CMDLINEFILE}"

    installGpsPackagesAndConfigs

    bootChanged=0
    cmdlineChanged=0

    if updateBootConfig; then
        bootChanged=1
        echo "Updated ${BOOTFILE}"
    else
        echo "No change needed in ${BOOTFILE}"
    fi

    if removeSerialConsoleFromCmdline "${CMDLINEFILE}"; then
        cmdlineChanged=1
    fi

    disableSerialGetty

    systemctl restart gpsd || true
    systemctl restart chrony || true

    if [[ "${bootChanged}" -eq 1 || "${cmdlineChanged}" -eq 1 ]]; then
        echo "Need to reboot for a full operational GPS/PPS setup"
    fi

    echo "End Installing GPS"
}

main "$@"