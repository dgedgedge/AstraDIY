# Guide pour pousser le code sur GitHub

## Prérequis
- Avoir l'URL du repository GitHub (ex: `https://github.com/user/astralim.git`)
- Avoir les droits d'écriture sur le repository
- La branche `V2` doit être créée sur GitHub (déjà fait selon vos indications)

## Étapes

### 1. Se placer dans le répertoire parent (Software)
```bash
cd "/Users/apple/Documents/Dev - Projets - hors Herd/astralim-hmi/AstraDIY/Software"
```

### 2. Vérifier si c'est déjà un repo Git
```bash
git status
```

### 3a. Si ce n'est PAS un repo Git, initialiser et configurer :
```bash
# Initialiser le repo
git init

# Ajouter le remote (remplacez par l'URL réelle)
git remote add origin https://github.com/USER/astralim.git

# Récupérer les branches existantes
git fetch origin

# Checkout la branche V2 (elle doit exister sur GitHub)
git checkout -b V2 origin/V2
# OU si la branche n'existe pas encore localement :
git checkout V2
```

### 3b. Si c'est DÉJÀ un repo Git :
```bash
# Vérifier le remote
git remote -v

# Si le remote n'est pas configuré, l'ajouter :
git remote add origin https://github.com/USER/astralim.git

# Récupérer les branches
git fetch origin

# Checkout la branche V2
git checkout V2
# OU créer la branche locale qui track la remote :
git checkout -b V2 origin/V2
```

### 4. Ajouter les fichiers du driver V2
```bash
# Se placer dans le répertoire indiDriverV2
cd indiDriverV2

# Ajouter tous les fichiers (le .gitignore exclura scripts_python/)
git add .

# Vérifier ce qui sera commité
git status
```

### 5. Faire le commit initial
```bash
git commit -m "feat: INDI Driver V2 - Migration vers API INDI 2.x

- Migration complète vers INDI API 2.x
- Support libgpiod v1 et v2
- Drivers séparés : Focuser, Relays, System, Heater
- Détection automatique des conflits GPIO
- Support INA219 pour monitoring courant/puissance
- Support PWM pour bandes chauffantes avec PID
- Compatible Raspberry Pi 5"
```

### 6. Pousser vers GitHub
```bash
# Si c'est la première fois sur cette branche
git push -u origin V2

# Sinon
git push origin V2
```

## Structure attendue dans le repo

Le code devrait être dans :
```
astralim/
├── Software/
│   ├── indiDriver/          # V1 (existant)
│   └── indiDriverV2/         # V2 (nouveau)
│       ├── CMakeLists.txt
│       ├── README.md
│       ├── deploy.sh
│       ├── *.cpp, *.h
│       ├── cmake_modules/
│       └── scripts_python/   # (ignoré par Git)
```

## En cas de problème

### Si la branche V2 n'existe pas encore sur GitHub :
```bash
# Créer la branche localement
git checkout -b V2

# Pousser et créer la branche sur GitHub
git push -u origin V2
```

### Si vous avez des conflits :
```bash
# Récupérer les dernières modifications
git fetch origin

# Rebaser votre travail
git rebase origin/V2

# Résoudre les conflits si nécessaire, puis :
git add .
git rebase --continue
```

