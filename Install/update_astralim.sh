#!/bin/bash
# Script de mise à jour AstraDIY
# Met à jour le clone git et relance l'installation

# Couleurs pour les messages
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "═══════════════════════════════════════════════════════════"
echo "         Mise à jour AstraDIY"
echo "═══════════════════════════════════════════════════════════"
echo ""

# Trouver le répertoire du projet (où se trouve ce script)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR=""

# Chercher le répertoire du projet dans plusieurs emplacements possibles
POSSIBLE_DIRS=(
    "$HOME/Documents/Dev - Projets - hors Herd/AstraDIY"
    "$HOME/Documents/AstraDIY"
    "$HOME/AstraDIY"
    "$SCRIPT_DIR/.."
    "$(dirname "$SCRIPT_DIR")"
)

for DIR in "${POSSIBLE_DIRS[@]}"; do
    if [ -d "$DIR/.git" ] && [ -f "$DIR/Install/install.sh" ]; then
        PROJECT_DIR="$DIR"
        break
    fi
done

if [ -z "$PROJECT_DIR" ]; then
    echo -e "${RED}❌ Erreur: Impossible de trouver le répertoire du projet AstraDIY${NC}"
    echo ""
    echo "Veuillez entrer le chemin complet vers le répertoire AstraDIY:"
    read -r PROJECT_DIR
    
    if [ ! -d "$PROJECT_DIR/.git" ] || [ ! -f "$PROJECT_DIR/Install/install.sh" ]; then
        echo -e "${RED}❌ Erreur: Répertoire invalide${NC}"
        echo "Appuyez sur Entrée pour fermer..."
        read
        exit 1
    fi
fi

echo -e "${GREEN}✓ Répertoire trouvé: $PROJECT_DIR${NC}"
echo ""

# Aller dans le répertoire du projet
cd "$PROJECT_DIR" || exit 1

# Étape 1: Mise à jour Git
echo -e "${YELLOW}Étape 1/2: Mise à jour du dépôt Git...${NC}"
echo ""

if ! command -v git &> /dev/null; then
    echo -e "${RED}❌ Git n'est pas installé${NC}"
    echo "Appuyez sur Entrée pour fermer..."
    read
    exit 1
fi

# Vérifier si on est dans une branche
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
if [ -z "$CURRENT_BRANCH" ]; then
    echo -e "${RED}❌ Ce répertoire n'est pas un dépôt Git valide${NC}"
    echo "Appuyez sur Entrée pour fermer..."
    read
    exit 1
fi

echo "Branche actuelle: $CURRENT_BRANCH"
echo "Récupération des mises à jour..."
git fetch origin

# Vérifier s'il y a des modifications locales
if [ -n "$(git status --porcelain)" ]; then
    echo -e "${YELLOW}⚠ Attention: Vous avez des modifications locales non commitées${NC}"
    echo "Voulez-vous les conserver ? (o/n)"
    read -r response
    if [[ "$response" =~ ^[Oo]$ ]]; then
        echo "Sauvegarde des modifications..."
        git stash
        STASHED=true
    else
        echo -e "${RED}Mise à jour annulée${NC}"
        echo "Appuyez sur Entrée pour fermer..."
        read
        exit 0
    fi
else
    STASHED=false
fi

# Faire le pull
echo ""
echo "Application des mises à jour..."
if git pull origin "$CURRENT_BRANCH"; then
    echo -e "${GREEN}✓ Mise à jour Git réussie${NC}"
else
    echo -e "${RED}❌ Erreur lors de la mise à jour Git${NC}"
    if [ "$STASHED" = true ]; then
        echo "Restauration des modifications sauvegardées..."
        git stash pop
    fi
    echo "Appuyez sur Entrée pour fermer..."
    read
    exit 1
fi

# Restaurer les modifications si elles ont été sauvegardées
if [ "$STASHED" = true ]; then
    echo "Restauration des modifications sauvegardées..."
    git stash pop
fi

echo ""
echo "═══════════════════════════════════════════════════════════"
echo ""

# Étape 2: Relancer l'installation
echo -e "${YELLOW}Étape 2/2: Relance de l'installation...${NC}"
echo ""

if [ ! -f "$PROJECT_DIR/Install/install.sh" ]; then
    echo -e "${RED}❌ Script d'installation non trouvé${NC}"
    echo "Appuyez sur Entrée pour fermer..."
    read
    exit 1
fi

echo "Lancement de l'installation..."
echo ""

# Lancer install.sh avec sudo si nécessaire
if [ "$EUID" -eq 0 ]; then
    bash "$PROJECT_DIR/Install/install.sh"
else
    echo "Cette opération nécessite les droits administrateur (sudo)."
    sudo bash "$PROJECT_DIR/Install/install.sh"
fi

INSTALL_EXIT_CODE=$?

echo ""
echo "═══════════════════════════════════════════════════════════"
echo ""

if [ $INSTALL_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ Mise à jour terminée avec succès !${NC}"
else
    echo -e "${RED}❌ Erreur lors de l'installation${NC}"
fi

echo ""
echo "Appuyez sur Entrée pour fermer..."
read

