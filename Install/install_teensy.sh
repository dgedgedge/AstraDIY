#!/bin/bash
FILESOURCE=$(dirname $0)
echo "Begin Installing teensy"
BOOTFILE=/boot/firmware/config.txt
if ! grep -q "uart3-pi5" ${BOOTFILE} ; then
    echo "L'option uart3-pi5 n'est pas dans le fichier /boot/firmware/config.txt. Ajout en cours..."
    cat >> ${BOOTFILE}  << "END1"

# Start TeenAstro
dtoverlay=uart3-pi5
# End TeenAstro

END1
   echo "Need to reboot for a full operational teensy"
fi

echo "End Installing teensy"

