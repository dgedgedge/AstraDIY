#!/bin/bash
# Script d'installation des drivers INDI AstrAlim
# Usage: ./install_indi_astralim.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=== Installation des drivers INDI AstrAlim ==="

# Installer les dépendances
echo "Installation des dépendances..."
sudo apt-get update
sudo apt-get install -y build-essential cmake libindi-dev libgpiod-dev pkg-config

# Nettoyer l'ancien build
echo "Nettoyage de l'ancien build..."
rm -rf "$BUILD_DIR"

# Build
echo "Compilation..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
make -j$(nproc)
sudo make install

# Redémarrer INDI si nécessaire
echo "Redémarrage des services INDI..."
sudo systemctl restart indiserver 2>/dev/null || sudo systemctl restart indi-web 2>/dev/null || true

echo "=== Installation terminée ==="

