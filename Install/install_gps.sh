#!/bin/bash
FILESOURCE=$(dirname $0)
echo "Begin Installing gps"
apt install -y gpsd chrony pps-tools
cp ${FILESOURCE}/gps/etc_default_gpsd /etc/default/gpsd
cp ${FILESOURCE}/gps/etc_chrony_conf.d_chronyastralim.conf /etc/chrony/conf.d/chronyastralim.conf
systemctl restart gpsd
systemctl restart chrony
BOOTFILE=/boot/firmware/config.txt
if ! grep -q "pps-gpio" ${BOOTFILE} ; then
    echo "L'option pps-gpio n'est pas dans le fichier /boot/firmware/config.txt. Ajout en cours..."
    cat >> ${BOOTFILE}  << "END1"

# === AstrAlim GPS ===
# Désactiver toute console série
enable_uart=1
# UART principal sur GPIO14 / GPIO15
dtoverlay=uart0
# PPS sur GPIO25
dtoverlay=pps-gpio,gpiopin=25
# Optionnel mais recommandé
dtparam=uart0_clkrate=48000000

END1
   echo "Need to reboot for a full operational gps"
fi

echo "End Installing gps"

