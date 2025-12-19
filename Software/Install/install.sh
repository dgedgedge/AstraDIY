#!/bin/env bash
FILESOURCE=$(dirname $0)

sudo ${FILESOURCE}/install_gps.sh
sudo ${FILESOURCE}/install_scripts.sh
sudo ${FILESOURCE}/install_bootConfig.sh
${FILESOURCE}/install_autostart.sh

# Installation du fichier .desktop pour les drivers INDI sur le bureau
echo "Installation du raccourci d'installation des drivers INDI sur le bureau..."
INDI_DRIVER_DIR="${FILESOURCE}/../indiDriver"
INDI_DESKTOP_SOURCE="${INDI_DRIVER_DIR}/Install_INDIAstrAlim.desktop"
INDI_WRAPPER_SCRIPT="${INDI_DRIVER_DIR}/Install_INDIAstrAlim_Desktop.sh"
DESKTOP_DIR="${HOME}/Desktop"
DESKTOP_DIR_ALT="${HOME}/Bureau"  # Pour les systèmes en français

if [ -f "$INDI_DESKTOP_SOURCE" ] && [ -f "$INDI_WRAPPER_SCRIPT" ]; then
    # Déterminer le répertoire du bureau
    TARGET_DESKTOP=""
    if [ -d "$DESKTOP_DIR" ]; then
        TARGET_DESKTOP="$DESKTOP_DIR"
    elif [ -d "$DESKTOP_DIR_ALT" ]; then
        TARGET_DESKTOP="$DESKTOP_DIR_ALT"
    fi
    
    if [ -n "$TARGET_DESKTOP" ]; then
        # Copier le script wrapper sur le bureau
        cp "$INDI_WRAPPER_SCRIPT" "$TARGET_DESKTOP/"
        chmod +x "$TARGET_DESKTOP/Install_INDIAstrAlim_Desktop.sh"
        
        # Créer un fichier .desktop adapté avec le bon chemin
        cat > "$TARGET_DESKTOP/Install_INDIAstrAlim.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Installer Drivers INDI AstrAlim
Comment=Compile et installe les drivers INDI pour AstrAlim
Exec=bash -c "cd \"$TARGET_DESKTOP\" && ./Install_INDIAstrAlim_Desktop.sh"
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
        echo "Vous pouvez le copier manuellement depuis: $INDI_DESKTOP_SOURCE"
    fi
else
    echo "Attention: Fichiers INDI non trouvés dans: $INDI_DRIVER_DIR"
fi

# i2c 
#i2cdetect  -y 1


