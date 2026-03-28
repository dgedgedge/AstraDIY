# AstraDIY
# Description :
This project is a documentation, Software Repository and archiving space for projects :
   * [AstrAlim](https://oshwlab.com/pololamag/astralim)
   * [AstrOnStep](https://oshwlab.com/pololamag/astronstep)
   * [HEMY](https://github.com/polvinc/HEMY)
   * TeenAstro-Redux
   * HEMY Break Driver

The main documentation is in the [associated wiki](https://github.com/dgedgedge/AstraDIY/wiki/WhatIsAstraDIY)
The spcific Software documentation are in the the sub directories of this project.

It contains :
   * the saved element for the hardware projet. See Hadrware directory.
   * Software developped scripts to simplify usage of the hardware. See Software diretory.

# Installation :
## Gps :
See [Installation process](Software/Install/README.md)
## Software Python Drivers:
### Manipulating GPIO :
   * AstraGpio.py
   * AstraGpioHmi.py
### Manage DrewHeater :
   * AstraPwm.py
   * AstraPwmHmi.py
### Monitoring Power consumption :
   * AstraIna.py
   * AstraInaHmi.py
## Usage with kstars/Indi:

# Installation complète du logiciel sur votre système

Cette section vous guide pas à pas pour installer tous les composants logiciels d'AstraDIY sur votre Raspberry Pi (ou système compatible).

## 📋 Prérequis

Avant de commencer, assurez-vous d'avoir :
- Un Raspberry Pi (testé sur Raspberry Pi 5 avec StellarMate 1.8)
- Un accès administrateur (sudo)
- Une connexion Internet pour télécharger les dépendances
- Les fichiers du projet AstraDIY téléchargés sur votre système

## 🚀 Installation complète (Méthode recommandée)

### Étape 1 : Préparer les fichiers

1. **Téléchargez ou clonez le projet AstraDIY** sur votre Raspberry Pi
   ```bash
   # Si vous utilisez git :
   cd ~
   git clone https://github.com/dgedgedge/AstraDIY.git AstraDIY
   cd AstraDIY
   
   # Ou téléchargez et décompressez l'archive dans un répertoire
   ```

2. **Ouvrez un terminal** et naviguez vers le répertoire `Install` :
   ```bash
   cd Install
   ```

### Étape 2 : Lancer l'installation complète

L'installation complète installe tous les composants en une seule commande :

```bash
sudo ./install.sh
```

Ce script va automatiquement :
- ✅ Installer et configurer le GPS (si présent)
- ✅ Installer les drivers Python et leurs dépendances
- ✅ Configurer le démarrage automatique des interfaces graphiques
- ✅ Configurer les paramètres de boot nécessaires (i2c, 1-Wire, PWM)
- ✅ Copier le raccourci d'installation des drivers INDI sur votre bureau

**⏱️ Durée estimée :** 5-10 minutes selon votre connexion Internet

**⚠️ Important :** Un redémarrage sera nécessaire après l'installation pour que tous les paramètres soient pris en compte.

---

## 🔧 Installation par composants (Méthode avancée)

Si vous préférez installer les composants individuellement, voici le détail de chaque étape :

### 1. Installation des drivers Python

Les drivers Python permettent de contrôler les différents modules matériels via des interfaces graphiques.

**Ce qui sera installé :**
- `AstraGpioHmi.py` : Interface pour contrôler les GPIO
- `AstraPwmHmi.py` : Interface pour gérer le chauffage (DrewHeater)
- `AstraInaHmi.py` : Interface pour surveiller la consommation électrique
- `AstraGpsHmi.py` : Interface pour surveiller le GPS
- `AstraDIYHmi.py` : Interface principale regroupant les fonctionnalités

**Procédure :**
```bash
cd Install
sudo ./install_scripts.sh
```

**Ce que fait ce script :**
1. Installe les dépendances Python nécessaires (PyQt5, libgpiod, etc.)
2. Copie tous les drivers Python dans `/opt/AstraDIY`
3. Configure les variables d'environnement pour que les scripts soient accessibles
4. Crée un script de démarrage automatique de toutes les interfaces

**📝 Note :** Les scripts seront accessibles depuis n'importe où après un redémarrage ou après avoir ouvert un nouveau terminal.

### 2. Configuration du démarrage automatique

Pour que les interfaces graphiques se lancent automatiquement au démarrage :

```bash
cd Install
./install_autostart.sh
```

**Ce que fait ce script :**
- Installe les fichiers `.desktop` dans `~/.config/autostart/`
- Les interfaces se lanceront automatiquement à chaque connexion utilisateur

**🎛️ Contrôle :**
- Vous pouvez désactiver le démarrage automatique en supprimant les fichiers de `~/.config/autostart/`
- Ou les modifier pour ne lancer que certaines interfaces

### 3. Configuration GPS (optionnel)

Si vous avez un module GPS connecté, installez sa configuration :

```bash
cd Install
sudo ./install_gps.sh
```

**Ce que fait ce script :**
- Installe et configure `gpsd` et `chrony` pour une synchronisation temporelle précise
- Configure le support PPS (Pulse Per Second) pour une précision microseconde
- Modifie `/boot/firmware/config.txt` pour activer les fonctionnalités GPS

**⚠️ Important :** Un redémarrage est nécessaire après cette installation.

### 4. Configuration du boot (paramètres matériels)

Pour activer les fonctionnalités matérielles nécessaires (i2c, 1-Wire, PWM) :

```bash
cd Install
sudo ./install_bootConfig.sh
```

**Ce que fait ce script :**
- Modifie `/boot/firmware/config.txt` pour activer :
  - I2C (communication avec les capteurs)
  - 1-Wire (capteurs de température)
  - PWM (contrôle du chauffage)

**⚠️ Important :** Un redémarrage est nécessaire après cette installation.

---

## 🔌 Installation des drivers INDI

Les drivers INDI permettent d'utiliser les modules AstrAlim avec des logiciels d'astronomie comme KStars, Ekos, etc.

### Méthode 1 : Installation graphique (Recommandée pour débutants)

**Le raccourci d'installation a été automatiquement copié sur votre bureau lors de l'installation complète** (`sudo ./install.sh`).

Si ce n'est pas le cas, ou si vous installez uniquement les drivers INDI :

1. **Naviguez vers le répertoire des drivers INDI :**
   ```bash
   cd Software/indiDriver
   ```

2. **Double-cliquez sur le fichier** `Install_INDIAstrAlim.desktop` sur votre bureau
   - Une fenêtre de terminal s'ouvrira
   - Le script vous guidera à travers toutes les étapes
   - Il vérifiera et installera automatiquement les dépendances
   - Compilera et installera les drivers

**📖 Pour plus de détails :** Consultez `Software/indiDriver/INSTALL_README.md`

### Méthode 2 : Installation depuis le terminal

```bash
cd Software/indiDriver
./install_indi_astralim.sh
```

**Ce que fait ce script :**
1. ✅ Vérifie la présence des dépendances (`build-essential`, `cmake`, `libindi-dev`, `libgpiod-dev`)
2. ✅ Installe automatiquement les dépendances manquantes
3. ✅ Configure le projet avec CMake
4. ✅ Compile les drivers INDI
5. ✅ Installe les drivers dans le système
6. ✅ Redémarre automatiquement le serveur INDI si nécessaire

**Drivers installés :**
- `indi_astralim_focuser` : Contrôle du moteur pas-à-pas (focuser)
- `indi_astralim_relays` : Contrôle des relais
- `indi_astralim_system` : Informations système
- `indi_astralim_heater` : Contrôle du chauffage

---

## ✅ Vérification de l'installation

### Vérifier les drivers Python

```bash
# Vérifier que les scripts sont accessibles
which AstraDIYHmi.py
# Devrait afficher : /opt/AstraDIY/AstraDIYHmi.py

# Tester l'exécution
AstraDIYHmi.py &
```

### Vérifier les drivers INDI

```bash
# Vérifier que les drivers sont installés
ls -l /usr/bin/indi_astralim_*

# Vérifier le fichier XML de description
ls -l $(pkg-config --variable=datadir indi)/indi/indi_astralim.xml
```

### Vérifier le serveur INDI

```bash
# Vérifier si le serveur INDI est actif
systemctl status indiserver
# ou
systemctl status indi-web
```

---

## 🔄 Redémarrage et finalisation

Après l'installation, **redémarrez votre système** pour que tous les paramètres soient pris en compte :

```bash
sudo reboot
```

Au redémarrage :
- ✅ Les interfaces graphiques Python devraient se lancer automatiquement
- ✅ Les drivers INDI seront disponibles dans KStars/Ekos
- ✅ Le GPS sera configuré et fonctionnel (si installé)

---

## 🆘 Dépannage

### Les scripts Python ne sont pas trouvés

```bash
# Vérifier que le fichier de configuration existe
cat /etc/profile.d/AstraDIY.sh

# Si absent, réinstaller :
cd Install
sudo ./install_scripts.sh

# Ouvrir un nouveau terminal ou se déconnecter/reconnecter
```

### Les drivers INDI n'apparaissent pas dans KStars

1. Vérifier que les drivers sont bien installés (voir section "Vérification")
2. Redémarrer le serveur INDI :
   ```bash
   sudo systemctl restart indiserver
   # ou
   sudo systemctl restart indi-web
   ```
3. Redémarrer KStars

### Les interfaces graphiques ne démarrent pas automatiquement

```bash
# Vérifier les fichiers d'autostart
ls -l ~/.config/autostart/Astra*.desktop

# Réinstaller l'autostart
cd Install
./install_autostart.sh
```

---

## 📚 Documentation complémentaire

- **Installation GPS :** Voir `Software/Install/README.md`
- **Drivers INDI :** Voir `Software/indiDriver/README.md` et `Software/indiDriver/INSTALL_README.md`
- **Wiki principal :** [WhatIsAstraDIY](https://github.com/dgedgedge/AstraDIY/wiki/WhatIsAstraDIY)

---

## Licences :
   * The current softwares are provided under the following [LICENSE](LICENSE)
   * Save for the following where the  authors either have not express the Licence and/or distributed with a similar licence :
      * The script included bme_lib.py is a 2024 copy of [bme280.py](https://github.com/awitwicki/MMM-BME280/blob/master/bme280.py) we are using as is.
      * The script ina219.py is a 2024 copy of [ina219.py](https://github.com/chrisb2/pi_ina219/blob/master/ina219.py) where we did modifiy the I2c access to be compatible with latest kernel and PI5.
      * The script syspwm.py is a 2024 copy of [syspwm.py](https://github.com/jdimpson/syspwm/blob/master/syspwm.py) where we did adapt to paths specific to PI5.

