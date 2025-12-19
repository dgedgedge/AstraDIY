#!/bin/env bash
FILESOURCE=$(dirname $0)

sudo ${FILESOURCE}/install_gps.sh
sudo ${FILESOURCE}/install_scripts.sh
sudo ${FILESOURCE}/install_bootConfig.sh
${FILESOURCE}/install_autostart.sh

# Installation du fichier .desktop pour les drivers INDI sur le bureau
echo "Installation du raccourci d'installation des drivers INDI sur le bureau..."
INDI_DRIVER_DIR="${FILESOURCE}/../Software/indiDriver"
INDI_INSTALL_SCRIPT="${INDI_DRIVER_DIR}/install_indi_astralim.sh"
DESKTOP_DIR="${HOME}/Desktop"
DESKTOP_DIR_ALT="${HOME}/Bureau"  # Pour les systèmes en français

if [ -f "$INDI_INSTALL_SCRIPT" ]; then
    # Déterminer le répertoire du bureau
    TARGET_DESKTOP=""
    if [ -d "$DESKTOP_DIR" ]; then
        TARGET_DESKTOP="$DESKTOP_DIR"
    elif [ -d "$DESKTOP_DIR_ALT" ]; then
        TARGET_DESKTOP="$DESKTOP_DIR_ALT"
    fi
    
    if [ -n "$TARGET_DESKTOP" ]; then
        # Créer un fichier .desktop qui appelle directement le script à son emplacement d'origine
        cat > "$TARGET_DESKTOP/Install_INDIAstrAlim.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Installer Drivers INDI AstrAlim
Comment=Compile et installe les drivers INDI pour AstrAlim
Exec=bash -c "cd \"${INDI_DRIVER_DIR}\" && sudo ./install_indi_astralim.sh"
Icon=application-x-executable
Terminal=true
Categories=System;Settings;
StartupNotify=true
Path=
EOF
        chmod +x "$TARGET_DESKTOP/Install_INDIAstrAlim.desktop"
        echo "Raccourci installé sur: $TARGET_DESKTOP/Install_INDIAstrAlim.desktop"
    else
        echo "Attention: Répertoire Bureau non trouvé. Le fichier .desktop n'a pas été copié."
        echo "Vous pouvez lancer manuellement: cd ${INDI_DRIVER_DIR} && sudo ./install_indi_astralim.sh"
    fi
else
    echo "Attention: Script d'installation INDI non trouvé dans: $INDI_DRIVER_DIR"
fi

# i2c 
#i2cdetect  -y 1


