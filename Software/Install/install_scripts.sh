#!/bin/bash
#
HOMEDIR=$(dirname $0)/..
INSTALLDIR=/opt/AstraDIY

echo "Remove Old dirs (AstrAlim)"
if [[ -d /opt/AstrAlim ]] ; then
	echo "Remove old directory /opt/AstrAlim"
	rm -rf /opt/AstrAlim  
fi
if [[ -e /etc/profile.d/AstrAlim.sh ]] ; then
	echo "Removing of setup /etc/profile.d/AstrAlim.sh"
	rm /etc/profile.d/AstrAlim.sh
fi

echo "Begin Installing scripts"
cat > /etc/profile.d/AstraDIY.sh << END
# Environnement Python AstraDIY 
export PYTHONPATH=\$PYTHONPATH:$INSTALLDIR
export PATH=\$PATH:$INSTALLDIR
END

echo "Install dependency"
sudo apt -y install python3 python3-libgpiod python3-smbus python3-pyqt5
sudo apt -y install gpiod i2c-tools  python3-ntplib python3-gps

# Installer les fichiers 
if [ -d  $INSTALLDIR ]
then
	rm -rf $INSTALLDIR
fi
mkdir -p $INSTALLDIR
for FILE in Astra*.py survDateOffset.py
do
	cp ${HOMEDIR}/pythonDrivers/${FILE} $INSTALLDIR
done
cp -r ${HOMEDIR}/pythonDrivers/lib $INSTALLDIR/
chmod a+rx  $INSTALLDIR/Astra*.py
chmod og-w $INSTALLDIR/Astra*.py

STARTALLSCRIPT=$INSTALLDIR/AstraStartAllHmi.sh
LISTHMI="AstraGpioHmi.py AstraPwmHmi.py AstraInaHmi.py AstraGpsHmi.py AstraDIYHmi.py"
echo "#!/bin/bash" > $STARTALLSCRIPT
for FILE in $LISTHMI
do
	echo "$INSTALLDIR/$FILE &" >> $STARTALLSCRIPT
done
chmod a+rx $STARTALLSCRIPT
chmod og-w $STARTALLSCRIPT

echo "Begin Installing scripts"

