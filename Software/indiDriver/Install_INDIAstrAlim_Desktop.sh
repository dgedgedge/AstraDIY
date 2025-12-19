#!/bin/bash
# Script à copier sur le bureau pour installer les drivers INDI AstrAlim
# Ce script trouve automatiquement le répertoire source

# Obtenir le répertoire où se trouve ce script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Chercher le répertoire source dans plusieurs emplacements possibles
POSSIBLE_DIRS=(
    "$HOME/Documents/Dev - Projets - hors Herd/AstraDIY/Software/indiDriver"
    "$HOME/Documents/AstraDIY/Software/indiDriver"
    "$HOME/AstraDIY/Software/indiDriver"
    "/opt/AstraDIY/Software/indiDriver"
    "$SCRIPT_DIR/../Software/indiDriver"
    "$SCRIPT_DIR/Software/indiDriver"
    "$(dirname "$SCRIPT_DIR")/Software/indiDriver"
)

SOURCE_DIR=""
for DIR in "${POSSIBLE_DIRS[@]}"; do
    if [ -f "$DIR/install_indi_astralim.sh" ]; then
        SOURCE_DIR="$DIR"
        break
    fi
done

# Si on n'a pas trouvé, demander à l'utilisateur
if [ -z "$SOURCE_DIR" ]; then
    echo "═══════════════════════════════════════════════════════════"
    echo "Impossible de trouver automatiquement le répertoire source."
    echo "═══════════════════════════════════════════════════════════"
    echo ""
    echo "Veuillez entrer le chemin complet vers le répertoire"
    echo "Software/indiDriver (ou appuyez sur Entrée pour annuler):"
    echo ""
    read -r SOURCE_DIR
    
    if [ -z "$SOURCE_DIR" ]; then
        echo "Installation annulée."
        echo "Appuyez sur Entrée pour fermer..."
        read
        exit 0
    fi
    
    if [ ! -f "$SOURCE_DIR/install_indi_astralim.sh" ]; then
        echo ""
        echo "Erreur: Le fichier install_indi_astralim.sh n'a pas été trouvé dans:"
        echo "$SOURCE_DIR"
        echo ""
        echo "Vérifiez que vous avez entré le bon chemin."
        echo "Appuyez sur Entrée pour fermer..."
        read
        exit 1
    fi
fi

# Lancer le script principal
echo "Répertoire source trouvé: $SOURCE_DIR"
echo "Lancement de l'installation..."
echo ""
cd "$SOURCE_DIR"
exec ./install_indi_astralim.sh

