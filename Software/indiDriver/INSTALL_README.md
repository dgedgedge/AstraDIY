# Installation des Drivers INDI AstrAlim

Ce guide explique comment installer les drivers INDI pour AstrAlim de manière simple.

## Méthode 1 : Double-clic depuis le bureau (Recommandé pour débutants)

**Note :** Si vous avez exécuté l'installation complète (`sudo ./install.sh` depuis le répertoire `Install`), le raccourci a déjà été automatiquement copié sur votre bureau et configuré.

### Lancer l'installation

1. Double-cliquez sur le fichier `Install_INDIAstrAlim.desktop` sur votre bureau
2. Une fenêtre de terminal s'ouvrira
3. Suivez les instructions à l'écran
4. Le script vous guidera à travers toutes les étapes :
   - Vérification et installation des dépendances
   - Configuration du projet
   - Compilation
   - Installation
   - Rechargement du serveur INDI

## Méthode 2 : Depuis le terminal

Ouvrez un terminal et exécutez :

```bash
cd /chemin/vers/AstraDIY/Software/indiDriver
./install_indi_astralim.sh
```

## Ce que fait le script

Le script `install_indi_astralim.sh` effectue les opérations suivantes de manière transparente :

1. **Vérification des dépendances** : Vérifie si les packages suivants sont installés :
   - `build-essential` (outils de compilation)
   - `cmake` (système de build)
   - `libindi-dev` (bibliothèque INDI)
   - `libgpiod-dev` (bibliothèque GPIO)

2. **Installation des dépendances** : Installe automatiquement les packages manquants (nécessite sudo)

3. **Nettoyage** : Supprime l'ancien répertoire de build s'il existe

4. **Configuration** : Configure le projet avec CMake

5. **Compilation** : Compile les drivers INDI (utilise tous les processeurs disponibles)

6. **Installation** : Installe les drivers dans le système (nécessite sudo)

7. **Rechargement** : Redémarre automatiquement le serveur INDI s'il est en cours d'exécution

## Drivers installés

Après l'installation, les drivers suivants seront disponibles :

- `indi_astralim_focuser` : Contrôle du moteur pas-à-pas (focuser)
- `indi_astralim_relays` : Contrôle des relais
- `indi_astralim_system` : Informations système
- `indi_astralim_heater` : Contrôle du chauffage

## Dépannage

### Le script ne s'exécute pas depuis le bureau

Si le double-clic ne fonctionne pas et que le fichier n'a pas été installé automatiquement :

1. Copiez manuellement le fichier `Install_INDIAstrAlim.desktop` sur votre bureau
2. Faites un clic droit sur le fichier → "Propriétés" → "Permissions"
3. Cochez "Autoriser l'exécution du fichier en tant que programme"
4. Ou utilisez la méthode 2 (terminal)

### Erreur de permissions

Si vous obtenez une erreur de permissions lors de l'installation :

```bash
sudo chmod +x install_indi_astralim.sh
```

### Le serveur INDI ne redémarre pas automatiquement

Si le script ne peut pas redémarrer le serveur INDI automatiquement, vous pouvez le faire manuellement :

```bash
sudo systemctl restart indiserver
# ou
sudo systemctl restart indi-web
```

## Support

Pour plus d'informations, consultez le fichier `README.md` dans le même répertoire.

