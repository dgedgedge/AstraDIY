#!/bin/bash
FILESOURCE=$(dirname $0)
echo "Begin Installing bootconfig"

BOOTFILE=/boot/firmware/config.txt

if ! egrep -q -e "^dtparam=i2c_arm=on" ${BOOTFILE} ; then
   echo "L'option dtparam=i2c_arm=on n'est pas dans le fichier /boot/firmware/config.txt. Ajout en cours..."
   sudo sed -i '/^#dtparam=i2c_arm=on/s/^#//' ${BOOTFILE}
   echo "Need to reboot for a full operational temperature sensor"
fi

if ! grep -q "w1-gpio" ${BOOTFILE} ; then
    echo "L'option w1-gpio n'est pas dans le fichier /boot/firmware/config.txt. Ajout en cours..."
    cat >> ${BOOTFILE}  << "END1"

# Begin AstrAlim 1 Wire
dtoverlay=w1-gpio
# End AstrAlim 1 wire

END1
   echo "Need to reboot for a full operational temperature sensor"
fi


if ! grep -q "pwm-2chan" ${BOOTFILE} ; then
    echo "L'option pwm-2chan n'est pas dans le fichier /boot/firmware/config.txt. Ajout en cours..."
    cat >> ${BOOTFILE}  << "END1"

# Begin AstrAlim PWM Outputs
dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4
# End AstrAlim PWM Outputs

END1
   echo "Need to reboot for a full operational pwm"
fi

if ! grep -q "AstrAlimPowerManagement" ${BOOTFILE} ; then
    echo "L'option AstrAlimPowerManagement n'est pas dans le fichier /boot/firmware/config.txt. Ajout en cours..."
    cat >> ${BOOTFILE}  << "END1"

# Begin AstrAlimPowerManagement  Outputs
usb_max_current_enable=1
PSU_MAX_CURRENT=5000
# End AstrAlimPowerManagement Outputs

END1
   echo "Need to reboot for a full operational pwm"
fi


echo "End Installing bootConfig"

