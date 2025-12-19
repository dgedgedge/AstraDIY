#!/bin/env bash
FILESOURCE=$(dirname $0)

sudo ${FILESOURCE}/install_gps.sh
sudo ${FILESOURCE}/install_scripts.sh
sudo ${FILESOURCE}/install_bootConfig.sh
${FILESOURCE}/install_autostart.sh

# Déterminer le répertoire du bureau
# Utiliser l'utilisateur réel (même si le script est lancé avec sudo)
REAL_USER="${SUDO_USER:-$USER}"
REAL_HOME=$(getent passwd "$REAL_USER" 2>/dev/null | cut -d: -f6)
if [ -z "$REAL_HOME" ]; then
    REAL_HOME="$HOME"
fi

DESKTOP_DIR="${REAL_HOME}/Desktop"
DESKTOP_DIR_ALT="${REAL_HOME}/Bureau"  # Pour les systèmes en français
TARGET_DESKTOP=""
if [ -d "$DESKTOP_DIR" ]; then
    TARGET_DESKTOP="$DESKTOP_DIR"
elif [ -d "$DESKTOP_DIR_ALT" ]; then
    TARGET_DESKTOP="$DESKTOP_DIR_ALT"
fi

# Installation de l'icône système
echo "Installation de l'icône système..."
LOGO_SOURCE="${FILESOURCE}/../Software/pythonDrivers/logo/logo-astralim-black-96x96.png"
ICON_DIR="${REAL_HOME}/.local/share/icons/hicolor/96x96/apps"
if [ -f "$LOGO_SOURCE" ]; then
    mkdir -p "$ICON_DIR"
    cp "$LOGO_SOURCE" "$ICON_DIR/astralim.png"
    echo "  ✓ Icône installée dans $ICON_DIR/astralim.png"
    # Mettre à jour le cache des icônes
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "${REAL_HOME}/.local/share/icons/hicolor" 2>/dev/null || true
    fi
    # Mettre à jour le cache LXDE si disponible
    if [ -d "${REAL_HOME}/.cache/lxpanel" ]; then
        touch "${REAL_HOME}/.cache/lxpanel" 2>/dev/null || true
    fi
    # Forcer la mise à jour du cache pour l'utilisateur réel
    if [ -n "$REAL_USER" ] && [ "$REAL_USER" != "root" ]; then
        sudo -u "$REAL_USER" gtk-update-icon-cache -f -t "${REAL_HOME}/.local/share/icons/hicolor" 2>/dev/null || true
    fi
else
    echo "  ⚠ Logo source non trouvé: $LOGO_SOURCE"
fi

# Installation des fichiers .desktop pour les HMI et les drivers INDI
if [ -n "$TARGET_DESKTOP" ]; then
    echo "Installation des raccourcis sur le bureau..."
    
    # Utiliser le nom de l'icône système (sans extension)
    ICON_NAME="astralim"
    
    # Fichier .desktop pour AstraDIY HMI
    cat > "$TARGET_DESKTOP/AstraDIY.desktop" << EOF
[Desktop Entry]
Type=Application
Name=AstraDIY
Comment=Panneau de contrôle AstraDIY (Bandes chauffantes, Alimentations, GPS)
Exec=/opt/AstraDIY/AstraDIYHmi.py
Icon=${ICON_NAME}
Terminal=false
Categories=System;Settings;
StartupNotify=true
StartupWMClass=AstraDIY
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
Icon=${ICON_NAME}
Terminal=false
Categories=System;Settings;
StartupNotify=true
StartupWMClass=AstraGPS
EOF
    chmod +x "$TARGET_DESKTOP/AstraGPS.desktop"
    gio set "$TARGET_DESKTOP/AstraGPS.desktop" metadata::trusted true 2>/dev/null || true
    echo "  ✓ AstraGPS.desktop installé"
    
    # Fichier .desktop pour la mise à jour
    UPDATE_SCRIPT="${FILESOURCE}/update_astralim.sh"
    if [ -f "$UPDATE_SCRIPT" ]; then
        # Convertir le chemin du script en chemin absolu
        UPDATE_SCRIPT_ABS=$(cd "$(dirname "$UPDATE_SCRIPT")" && pwd)/$(basename "$UPDATE_SCRIPT")
        
        # Créer le fichier .desktop pour la mise à jour
        cat > "$TARGET_DESKTOP/Mise à Jour Astralim.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Mise à Jour Astralim
Comment=Met à jour le dépôt Git et relance l'installation AstraDIY
Exec=bash "${UPDATE_SCRIPT_ABS}"
Icon=${ICON_NAME}
Terminal=true
Categories=System;Settings;
StartupNotify=true
Path=
EOF
        chmod +x "$TARGET_DESKTOP/Mise à Jour Astralim.desktop"
        gio set "$TARGET_DESKTOP/Mise à Jour Astralim.desktop" metadata::trusted true 2>/dev/null || true
        echo "  ✓ Mise à Jour Astralim.desktop installé"
    else
        echo "  ⚠ Script de mise à jour non trouvé: $UPDATE_SCRIPT"
    fi
    
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
Icon=${ICON_NAME}
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


