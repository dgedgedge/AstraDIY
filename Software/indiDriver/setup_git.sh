#!/bin/bash
# Script pour configurer Git et pousser indiDriverV2 sur GitHub
# Usage: ./setup_git.sh [github-repo-url]
# Par défaut: git@github.com:dgedgedge/AstraDIY.git

set -e

# URL par défaut du repo
DEFAULT_REPO_URL="git@github.com:dgedgedge/AstraDIY.git"

if [ -z "$1" ]; then
    REPO_URL="$DEFAULT_REPO_URL"
    echo "Utilisation de l'URL par défaut: $REPO_URL"
else
    REPO_URL="$1"
fi
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARENT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Configuration Git pour indiDriverV2 ==="
echo "Repository: $REPO_URL"
echo "Répertoire parent: $PARENT_DIR"
echo ""

# Se placer dans le répertoire parent
cd "$PARENT_DIR"

# Vérifier si c'est déjà un repo Git
if [ -d ".git" ]; then
    echo "✓ Répertoire Git détecté"
    
    # Vérifier le remote
    if git remote get-url origin >/dev/null 2>&1; then
        CURRENT_REMOTE=$(git remote get-url origin)
        echo "  Remote actuel: $CURRENT_REMOTE"
        
        if [ "$CURRENT_REMOTE" != "$REPO_URL" ]; then
            read -p "Le remote actuel est différent. Voulez-vous le changer? (y/N) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                git remote set-url origin "$REPO_URL"
                echo "  ✓ Remote mis à jour"
            fi
        fi
    else
        echo "  Ajout du remote..."
        git remote add origin "$REPO_URL"
        echo "  ✓ Remote ajouté"
    fi
else
    echo "Initialisation du répertoire Git..."
    git init
    git remote add origin "$REPO_URL"
    echo "✓ Répertoire Git initialisé"
fi

# Récupérer les branches distantes
echo ""
echo "Récupération des branches distantes..."
git fetch origin || {
    echo "⚠ Impossible de récupérer les branches. La branche V2 existe-t-elle sur GitHub?"
    echo "  Vous pouvez la créer avec: git checkout -b V2 && git push -u origin V2"
    exit 1
}

# Vérifier si la branche V2 existe
if git show-ref --verify --quiet refs/remotes/origin/V2; then
    echo "✓ Branche V2 trouvée sur le remote"
    
    # Checkout ou créer la branche locale
    if git show-ref --verify --quiet refs/heads/V2; then
        echo "  Branche locale V2 existe déjà"
        git checkout V2
    else
        echo "  Création de la branche locale V2 qui track origin/V2"
        git checkout -b V2 origin/V2
    fi
else
    echo "⚠ Branche V2 non trouvée sur le remote"
    read -p "Voulez-vous créer la branche V2? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git checkout -b V2
        echo "  ✓ Branche V2 créée localement"
    else
        echo "  Abandon. Créez la branche V2 sur GitHub d'abord."
        exit 1
    fi
fi

# Se placer dans indiDriverV2
cd "$SCRIPT_DIR"

# Vérifier le statut
echo ""
echo "=== Statut Git ==="
git status --short

# Afficher les fichiers qui seront ajoutés
echo ""
echo "=== Fichiers à ajouter ==="
git status --porcelain | grep "^??" | head -20 || echo "Aucun nouveau fichier"

# Demander confirmation
echo ""
read -p "Voulez-vous ajouter tous les fichiers et faire un commit? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Abandon. Vous pouvez continuer manuellement avec:"
    echo "  cd $SCRIPT_DIR"
    echo "  git add ."
    echo "  git commit -m 'feat: INDI Driver V2'"
    echo "  git push origin V2"
    exit 0
fi

# Ajouter les fichiers
echo ""
echo "Ajout des fichiers..."
git add .

# Faire le commit
COMMIT_MSG="feat: INDI Driver V2 - Migration vers API INDI 2.x

- Migration complète vers INDI API 2.x
- Support libgpiod v1 et v2
- Drivers séparés : Focuser, Relays, System, Heater
- Détection automatique des conflits GPIO
- Support INA219 pour monitoring courant/puissance
- Support PWM pour bandes chauffantes avec PID
- Compatible Raspberry Pi 5"

echo ""
echo "Création du commit..."
git commit -m "$COMMIT_MSG" || {
    echo "⚠ Aucun changement à commiter (fichiers déjà commités?)"
    exit 0
}

# Demander confirmation pour push
echo ""
read -p "Voulez-vous pousser vers GitHub maintenant? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Commit créé localement. Pour pousser plus tard:"
    echo "  git push origin V2"
    exit 0
fi

# Pousser
echo ""
echo "Push vers GitHub..."
git push -u origin V2

echo ""
echo "✓ Terminé! Le code est maintenant sur GitHub dans la branche V2"
echo "  URL: $REPO_URL/tree/V2/Software/indiDriverV2"

