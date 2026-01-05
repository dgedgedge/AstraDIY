# Scripts de test et diagnostic

Ce répertoire contient tous les scripts de test et de diagnostic pour le système AstraDIY.

## Scripts disponibles

### Tests GPS
- **`test_gps_step_by_step.py`** : Test complet pas à pas du système GPS
  - Vérifie l'overlay PPS, le périphérique /dev/pps0, gpsd, la connexion GPS, etc.
  - Usage: `python3 test_gps_step_by_step.py`

### Tests PWM
- **`test_pwm.py`** : Test de base des canaux PWM avec détection automatique
- **`test_pwm_channels_direct.py`** : Test direct des canaux PWM et vérification INA
- **`test_pwm_gpio_mapping.py`** : Vérification du mapping GPIO vers PWM
- **`check_pwm_overlay.py`** : Vérification de la configuration de l'overlay PWM
- **`check_gpio_pwm_mapping.py`** : Vérification du mapping GPIO/PWM via device tree

### Tests INA219
- **`test_ina_diagnostic.py`** : Diagnostic des capteurs INA219 pour les bandes chauffantes
- **`scan_ina219.py`** : Scan de toutes les adresses INA219 sur le bus I2C

## Utilisation

Tous les scripts peuvent être exécutés depuis le répertoire `tests/` ou depuis le répertoire d'installation `/opt/AstraDIY/` après installation.

Les scripts ajoutent automatiquement le répertoire parent au `sys.path` pour pouvoir importer les modules `lib.*` et `Astra*`.

## Installation

Les scripts sont automatiquement copiés dans `/opt/AstraDIY/` lors de l'installation via `install_scripts.sh`.

