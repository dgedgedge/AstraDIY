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
PYTHONDRIVERSDIR=${HOMEDIR}/Software/pythonDrivers
# Copier les fichiers Astra*.py
for FILE in ${PYTHONDRIVERSDIR}/Astra*.py
do
	if [ -f "$FILE" ]; then
		cp "$FILE" $INSTALLDIR
	fi
done
# Copier les autres fichiers spécifiques
# Note: ina219.py, syspwm.py et bme280_lib.py sont maintenant dans lib/ et seront copiés avec le répertoire lib/
for FILE in survDateOffset.py
do
	if [ -f ${PYTHONDRIVERSDIR}/${FILE} ]; then
		cp ${PYTHONDRIVERSDIR}/${FILE} $INSTALLDIR
	fi
done
# Copier les fichiers de diagnostic depuis le répertoire tests/ (optionnel, pour le débogage)
if [ -d "${PYTHONDRIVERSDIR}/tests" ]; then
	echo "Copie des scripts de test depuis tests/..."
	for FILE in ${PYTHONDRIVERSDIR}/tests/*.py
	do
		if [ -f "$FILE" ]; then
			cp "$FILE" $INSTALLDIR
			chmod a+rx "$INSTALLDIR/$(basename "$FILE")"
		fi
	done
fi

# Copier les répertoires logo et lib (avec leur structure complète)
if [ -d "${PYTHONDRIVERSDIR}/logo" ]; then
	echo "Copie du répertoire logo/ (avec son contenu)..."
	cp -r ${PYTHONDRIVERSDIR}/logo $INSTALLDIR/
fi
if [ -d "${PYTHONDRIVERSDIR}/lib" ]; then
	echo "Copie du répertoire lib/ (avec son contenu)..."
	cp -r ${PYTHONDRIVERSDIR}/lib $INSTALLDIR/
fi

chmod a+rx  $INSTALLDIR/Astra*.py
chmod og-w $INSTALLDIR/Astra*.py

STARTALLSCRIPT=$INSTALLDIR/AstraStartAllHmi.sh
echo "Begin Installing scripts"

# Créer le script de démarrage qui lance uniquement AstraDIYHmi.py
# (qui contient tous les onglets : PwmH, Gpio, Ina)
echo "#!/bin/bash" > $STARTALLSCRIPT
echo "$INSTALLDIR/AstraDIYHmi.py &" >> $STARTALLSCRIPT
chmod a+rx $STARTALLSCRIPT
chmod og-w $STARTALLSCRIPT

echo ""
echo "=== Vérification de l'installation ==="
ERRORS=0
WARNINGS=0

# Vérifier que le répertoire d'installation existe
if [ ! -d "$INSTALLDIR" ]; then
	echo "❌ ERREUR: Le répertoire $INSTALLDIR n'existe pas"
	ERRORS=$((ERRORS + 1))
else
	echo "✓ Répertoire $INSTALLDIR créé"
fi

# Vérifier les fichiers Python principaux
echo ""
echo "Vérification des fichiers Python..."
EXPECTED_FILES="AstraGpioHmi.py AstraPwmHmi.py AstraInaHmi.py AstraGpsHmi.py AstraDIYHmi.py survDateOffset.py"
for FILE in $EXPECTED_FILES
do
	if [ -f "$INSTALLDIR/$FILE" ]; then
		echo "  ✓ $FILE"
	else
		echo "  ❌ $FILE - MANQUANT"
		ERRORS=$((ERRORS + 1))
	fi
done

# Vérifier les autres fichiers Astra*.py
echo ""
echo "Vérification des autres fichiers Astra*.py..."
ASTRA_FILES=$(ls $INSTALLDIR/Astra*.py 2>/dev/null | wc -l)
if [ "$ASTRA_FILES" -gt 0 ]; then
	echo "  ✓ $ASTRA_FILES fichier(s) Astra*.py trouvé(s)"
	for FILE in $INSTALLDIR/Astra*.py
	do
		BASENAME=$(basename "$FILE")
		if [[ ! "$EXPECTED_FILES" =~ "$BASENAME" ]]; then
			echo "    - $BASENAME"
		fi
	done
else
	echo "  ⚠ Aucun fichier Astra*.py trouvé"
	WARNINGS=$((WARNINGS + 1))
fi

# Vérifier le script de démarrage
echo ""
echo "Vérification du script de démarrage..."
if [ -f "$STARTALLSCRIPT" ]; then
	echo "  ✓ $STARTALLSCRIPT créé"
	if [ -x "$STARTALLSCRIPT" ]; then
		echo "  ✓ Script exécutable"
	else
		echo "  ⚠ Script non exécutable"
		WARNINGS=$((WARNINGS + 1))
	fi
else
	echo "  ❌ $STARTALLSCRIPT - MANQUANT"
	ERRORS=$((ERRORS + 1))
fi

# Vérifier les répertoires logo et lib
echo ""
echo "Vérification des répertoires..."
if [ -d "$INSTALLDIR/logo" ]; then
	echo "  ✓ Répertoire logo/ copié"
	LOGO_FILES=$(ls $INSTALLDIR/logo/* 2>/dev/null | wc -l)
	if [ "$LOGO_FILES" -gt 0 ]; then
		echo "    - $LOGO_FILES fichier(s) trouvé(s)"
	else
		echo "    ⚠ Répertoire logo/ vide"
		WARNINGS=$((WARNINGS + 1))
	fi
else
	echo "  ⚠ Répertoire logo/ - MANQUANT"
	WARNINGS=$((WARNINGS + 1))
fi

if [ -d "$INSTALLDIR/lib" ]; then
	echo "  ✓ Répertoire lib/ copié"
	LIB_FILES=$(ls $INSTALLDIR/lib/*.py 2>/dev/null | wc -l)
	if [ "$LIB_FILES" -gt 0 ]; then
		echo "    - $LIB_FILES fichier(s) Python trouvé(s)"
	else
		echo "    ⚠ Répertoire lib/ vide ou sans fichiers Python"
		WARNINGS=$((WARNINGS + 1))
	fi
else
	echo "  ⚠ Répertoire lib/ - MANQUANT"
	WARNINGS=$((WARNINGS + 1))
fi

# Vérifier le fichier de configuration d'environnement
echo ""
echo "Vérification de la configuration d'environnement..."
if [ -f "/etc/profile.d/AstraDIY.sh" ]; then
	echo "  ✓ /etc/profile.d/AstraDIY.sh créé"
else
	echo "  ⚠ /etc/profile.d/AstraDIY.sh - MANQUANT"
	WARNINGS=$((WARNINGS + 1))
fi

# Résumé final
echo ""
echo "=========================================="
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
	echo "✅ Installation réussie !"
	echo ""
	echo "Les fichiers ont été installés dans : $INSTALLDIR"
	echo "Le script de démarrage est disponible : $STARTALLSCRIPT"
	echo ""
	echo "Pour appliquer les variables d'environnement, exécutez :"
	echo "  source /etc/profile.d/AstraDIY.sh"
	echo "Ou redémarrez votre session."
	exit 0
elif [ $ERRORS -eq 0 ]; then
	echo "⚠ Installation terminée avec $WARNINGS avertissement(s)"
	echo "Vérifiez les messages ci-dessus pour plus de détails."
	exit 0
else
	echo "❌ Installation échouée avec $ERRORS erreur(s) et $WARNINGS avertissement(s)"
	echo "Vérifiez les messages ci-dessus et corrigez les problèmes."
	exit 1
fi

