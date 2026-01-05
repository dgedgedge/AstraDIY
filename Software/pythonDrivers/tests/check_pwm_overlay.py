#!/usr/bin/env python3
"""
Script pour vérifier la configuration de l'overlay pwm-2chan
"""

import subprocess
import os

def check_overlay_config():
    """Vérifie la configuration de l'overlay dans config.txt"""
    print("="*60)
    print("  Vérification de l'overlay pwm-2chan")
    print("="*60)
    
    config_files = [
        "/boot/firmware/config.txt",
        "/boot/config.txt"
    ]
    
    for config_file in config_files:
        if os.path.exists(config_file):
            print(f"\n✓ Fichier trouvé: {config_file}")
            with open(config_file, 'r') as f:
                content = f.read()
                if "pwm-2chan" in content:
                    print("  Configuration pwm-2chan trouvée:")
                    for line in content.split('\n'):
                        if "pwm-2chan" in line:
                            print(f"    {line.strip()}")
                else:
                    print("  ❌ Configuration pwm-2chan NON trouvée!")
        else:
            print(f"\n✗ Fichier non trouvé: {config_file}")

def check_dmesg_pwm():
    """Vérifie les messages dmesg concernant PWM"""
    print("\n" + "="*60)
    print("  Messages dmesg concernant PWM")
    print("="*60)
    
    try:
        result = subprocess.run(['dmesg'], capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            pwm_lines = [line for line in result.stdout.split('\n') if 'pwm' in line.lower() or '18' in line or '13' in line]
            if pwm_lines:
                print("  Messages trouvés:")
                for line in pwm_lines[-20:]:  # Derniers 20 messages
                    print(f"    {line}")
            else:
                print("  Aucun message PWM trouvé dans dmesg")
    except Exception as e:
        print(f"  Erreur lors de la lecture dmesg: {e}")

def check_gpio_pinctrl():
    """Vérifie le pinctrl pour GPIO 18 et 13"""
    print("\n" + "="*60)
    print("  Vérification pinctrl pour GPIO 18 et 13")
    print("="*60)
    
    gpios_to_check = [13, 18]
    
    for gpio in gpios_to_check:
        print(f"\n--- GPIO {gpio} ---")
        try:
            # Vérifier via gpioinfo si disponible
            result = subprocess.run(['gpioinfo'], capture_output=True, text=True, timeout=2)
            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if f'line {gpio}' in line.lower() or f'gpio{gpio}' in line.lower():
                        print(f"  {line}")
        except:
            pass
        
        # Vérifier via /sys/kernel/debug/pinctrl
        pinctrl_base = "/sys/kernel/debug/pinctrl"
        if os.path.exists(pinctrl_base):
            for pinctrl_dir in os.listdir(pinctrl_base):
                if "pwm" in pinctrl_dir.lower():
                    pinctrl_path = os.path.join(pinctrl_base, pinctrl_dir)
                    pinmux_path = os.path.join(pinctrl_path, "pinmux-pins")
                    if os.path.exists(pinmux_path):
                        try:
                            with open(pinmux_path, 'r') as f:
                                for line in f:
                                    if f'pin {gpio}' in line.lower() or f'pin ({gpio})' in line.lower():
                                        print(f"  {line.strip()}")
                        except:
                            pass

def check_pwm_channels():
    """Vérifie quels canaux PWM sont disponibles"""
    print("\n" + "="*60)
    print("  Canaux PWM disponibles")
    print("="*60)
    
    pwmchip_path = "/sys/class/pwm/pwmchip0"
    if os.path.exists(pwmchip_path):
        npwm_path = os.path.join(pwmchip_path, "npwm")
        if os.path.exists(npwm_path):
            with open(npwm_path, 'r') as f:
                npwm = int(f.read().strip())
            print(f"  pwmchip0 a {npwm} canaux")
            
            for ch in range(min(npwm, 4)):
                pwm_path = os.path.join(pwmchip_path, f"pwm{ch}")
                if os.path.exists(pwm_path):
                    print(f"  ✓ Canal {ch} exporté")
                else:
                    print(f"  ✗ Canal {ch} non exporté")

def main():
    check_overlay_config()
    check_dmesg_pwm()
    check_gpio_pinctrl()
    check_pwm_channels()
    
    print("\n" + "="*60)
    print("  DIAGNOSTIC")
    print("="*60)
    print("\nSi aucun canal ne fonctionne pour GPIO 18 (INA 0x49),")
    print("le problème peut être:")
    print("  1. L'overlay pwm-2chan ne configure pas correctement GPIO 18")
    print("  2. GPIO 18 n'est pas connecté matériellement à l'INA 0x49")
    print("  3. L'overlay doit être rechargé (redémarrage nécessaire)")
    print("="*60)

if __name__ == "__main__":
    main()

