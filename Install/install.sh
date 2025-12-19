#!/bin/env bash
FILESOURCE=$(dirname $0)

sudo ${FILESOURCE}/install_gps.sh
sudo ${FILESOURCE}/install_scripts.sh
sudo ${FILESOURCE}/install_bootConfig.sh
${FILESOURCE}/install_autostart.sh

# Déterminer le répertoire du bureau
DESKTOP_DIR="${HOME}/Desktop"
DESKTOP_DIR_ALT="${HOME}/Bureau"  # Pour les systèmes en français
TARGET_DESKTOP=""
if [ -d "$DESKTOP_DIR" ]; then
    TARGET_DESKTOP="$DESKTOP_DIR"
elif [ -d "$DESKTOP_DIR_ALT" ]; then
    TARGET_DESKTOP="$DESKTOP_DIR_ALT"
fi

# Installation des fichiers .desktop pour les HMI et les drivers INDI
if [ -n "$TARGET_DESKTOP" ]; then
    echo "Installation des raccourcis sur le bureau..."
    
    # Chemin vers le logo (installé dans /opt/AstraDIY/logo/)
    LOGO_PATH="/opt/AstraDIY/logo/logo-astralim-black.jpg"
    
    # Fichier .desktop pour AstraDIY HMI
    cat > "$TARGET_DESKTOP/AstraDIY.desktop" << EOF
[Desktop Entry]
Type=Application
Name=AstraDIY
Comment=Panneau de contrôle AstraDIY (Bandes chauffantes, Alimentations, GPS)
Exec=/opt/AstraDIY/AstraDIYHmi.py
Icon=${LOGO_PATH}
Terminal=false
Categories=System;Settings;
StartupNotify=true
EOF
    chmod +x "$TARGET_DESKTOP/AstraDIY.desktop"
    gio set "$TARGET_DESKTOP/AstraDIY.desktop" metadata::trusted true 2>/dev/null || true
    echo "  ✓ AstraDIY.desktop installé"
    
    # Fichier .desktop pour AstraGPS HMI
    cat > "$TARGET_DESKTOP/AstraGPS.desktop" << EOF
[Desktop Entry]
Type=Application
Name=AstraGPS
Comment=Interface GPS et Horloge AstraDIY
Exec=/opt/AstraDIY/AstraGpsHmi.py
Icon=${LOGO_PATH}
Terminal=false
Categories=System;Settings;
StartupNotify=true
EOF
    chmod +x "$TARGET_DESKTOP/AstraGPS.desktop"
    gio set "$TARGET_DESKTOP/AstraGPS.desktop" metadata::trusted true 2>/dev/null || true
    echo "  ✓ AstraGPS.desktop installé"
    
    # Fichier .desktop pour l'installation des drivers INDI
    INDI_DRIVER_DIR="${FILESOURCE}/../Software/indiDriver"
    # Convertir en chemin absolu
    INDI_DRIVER_DIR_ABS=$(cd "$INDI_DRIVER_DIR" && pwd)
    INDI_INSTALL_SCRIPT="${INDI_DRIVER_DIR_ABS}/install_indi_astralim.sh"
    if [ -f "$INDI_INSTALL_SCRIPT" ]; then
        cat > "$TARGET_DESKTOP/Install_INDIAstrAlim.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Installer Drivers INDI AstrAlim
Comment=Compile et installe les drivers INDI pour AstrAlim
Exec=bash -c "cd \"${INDI_DRIVER_DIR_ABS}\" && sudo ./install_indi_astralim.sh"
Icon=${LOGO_PATH}
Terminal=true
Categories=System;Settings;
StartupNotify=true
Path=
EOF
        chmod +x "$TARGET_DESKTOP/Install_INDIAstrAlim.desktop"
        gio set "$TARGET_DESKTOP/Install_INDIAstrAlim.desktop" metadata::trusted true 2>/dev/null || true
        echo "  ✓ Install_INDIAstrAlim.desktop installé"
    else
        echo "  ⚠ Script d'installation INDI non trouvé dans: $INDI_DRIVER_DIR_ABS"
    fi
else
    echo "Attention: Répertoire Bureau non trouvé. Les fichiers .desktop n'ont pas été installés."
    echo "Vous pouvez les créer manuellement ou lancer directement:"
    echo "  - /opt/AstraDIY/AstraDIYHmi.py"
    echo "  - /opt/AstraDIY/AstraGpsHmi.py"
fi

# i2c 
#i2cdetect  -y 1


