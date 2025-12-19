# AstrAlim INDI Driver v3

Driver INDI modernisé pour la carte AstrAlim sur Raspberry Pi 5.

## Prérequis

Sur le RPi5/StellarMate :
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libindi-dev libgpiod-dev
```

## Compilation

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

## Modules

- **AstrAlim Focuser** : Contrôle moteur pas-à-pas (DRV8834/A4988)
- **AstrAlim Relays** : 3 sorties DC commutables
- **AstrAlim System** : Infos système RPi

## GPIO (BCM - RPi5)

| Fonction | Pin |
|----------|-----|
| DIR      | 10  |
| STEP     | 24  |
| SLEEP    | 23  |
| M1       | 11  |
| M2       | 7   |
| M3       | 5   |
| DC1      | 26  |
| DC2      | 20  |
| DC3      | 21  |

## Changements v3

- API INDI moderne (PropertyNumber, PropertySwitch)
- Support libgpiod v1 et v2
- Compatibilité RPi5 (/dev/gpiochip4)
- C++17

