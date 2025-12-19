#!/bin/bash
# Wrapper pour lancer le script d'installation depuis le bureau
# Ce script trouve automatiquement le répertoire du script principal

# Trouver le répertoire où se trouve ce script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Lancer le script principal
"$SCRIPT_DIR/install_indi_astralim.sh"

