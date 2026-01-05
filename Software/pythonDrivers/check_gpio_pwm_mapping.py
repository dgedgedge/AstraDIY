#!/usr/bin/env python3
"""
Script de diagnostic pour vérifier le mapping GPIO -> PWM
"""

import os
import glob
import subprocess

def check_pinctrl_gpio(gpio_num):
    """Vérifie la fonction pinctrl d'un GPIO"""
    try:
        # Vérifier les fonctions disponibles
        funcs_path = f"/sys/kernel/debug/pinctrl/1f00098000.pwm/gpio-ranges"
        if os.path.exists(funcs_path):
            with open(funcs_path, 'r') as f:
                content = f.read()
                print(f"  GPIO {gpio_num} pinctrl info:\n{content}")
        
        # Vérifier via gpioinfo si disponible
        try:
            result = subprocess.run(['gpioinfo'], capture_output=True, text=True, timeout=2)
            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if f'gpiochip' in line.lower() or f'line {gpio_num}' in line.lower():
                        print(f"  GPIO {gpio_num} info: {line}")
        except:
            pass
            
        # Vérifier via /sys/class/gpio si exporté
        gpio_path = f"/sys/class/gpio/gpio{gpio_num}"
        if os.path.exists(gpio_path):
            direction_path = os.path.join(gpio_path, "direction")
            if os.path.exists(direction_path):
                with open(direction_path, 'r') as f:
                    direction = f.read().strip()
                    print(f"  GPIO {gpio_num} direction: {direction}")
    except Exception as e:
        print(f"  Erreur lors de la vérification GPIO {gpio_num}: {e}")

def check_pwmchip_channels(chip_num):
    """Vérifie les canaux disponibles dans un pwmchip"""
    chip_path = f"/sys/class/pwm/pwmchip{chip_num}"
    if not os.path.exists(chip_path):
        print(f"  pwmchip{chip_num} n'existe pas")
        return
    
    # Lire npwm
    npwm_path = os.path.join(chip_path, "npwm")
    if os.path.exists(npwm_path):
        with open(npwm_path, 'r') as f:
            npwm = int(f.read().strip())
        print(f"  pwmchip{chip_num} a {npwm} canaux")
    
    # Vérifier les canaux exportés
    for ch in range(npwm):
        pwm_path = os.path.join(chip_path, f"pwm{ch}")
        if os.path.exists(pwm_path):
            print(f"  Canal {ch} exporté: {pwm_path}")
            # Lire les propriétés si possible
            for prop in ["period", "duty_cycle", "enable", "polarity"]:
                prop_path = os.path.join(pwm_path, prop)
                if os.path.exists(prop_path):
                    try:
                        with open(prop_path, 'r') as f:
                            value = f.read().strip()
                            print(f"    {prop}: {value}")
                    except:
                        pass

def check_device_tree():
    """Vérifie la configuration device tree"""
    print("\n=== Configuration Device Tree ===")
    
    # Vérifier le modèle
    model_path = "/sys/firmware/devicetree/base/model"
    if os.path.exists(model_path):
        with open(model_path, 'r') as f:
            model = f.read().strip()
        print(f"Modèle: {model}")
    
    # Vérifier les overlays chargés
    overlays_path = "/sys/kernel/debug/pinctrl/*/gpio-ranges"
    overlays = glob.glob(overlays_path)
    if overlays:
        print(f"Overlays pinctrl trouvés: {overlays}")
        for ov in overlays[:3]:  # Limiter à 3
            try:
                with open(ov, 'r') as f:
                    print(f"  {ov}: {f.read()[:200]}")
            except:
                pass

def check_pwm_mapping():
    """Vérifie le mapping PWM réel"""
    print("\n=== Mapping PWM ===")
    
    # Trouver tous les pwmchips
    pwmchips = sorted(glob.glob("/sys/class/pwm/pwmchip*"))
    print(f"PWM chips trouvés: {len(pwmchips)}")
    
    for chip_path in pwmchips:
        chip_num = int(chip_path.replace("/sys/class/pwm/pwmchip", ""))
        print(f"\n--- pwmchip{chip_num} ---")
        check_pwmchip_channels(chip_num)
        
        # Vérifier le lien symbolique pour comprendre la source
        real_path = os.path.realpath(chip_path)
        print(f"  Chemin réel: {real_path}")
        
        # Extraire le nom du périphérique
        if "pwm" in real_path:
            parts = real_path.split("/")
            for part in parts:
                if "pwm" in part.lower():
                    print(f"  Périphérique: {part}")

def check_gpio_pwm_connection():
    """Vérifie la connexion GPIO -> PWM"""
    print("\n=== Connexion GPIO -> PWM ===")
    
    gpios_to_check = [13, 18]
    
    for gpio in gpios_to_check:
        print(f"\n--- GPIO {gpio} ---")
        check_pinctrl_gpio(gpio)
        
        # Vérifier via dmesg si disponible
        try:
            result = subprocess.run(['dmesg'], capture_output=True, text=True, timeout=2)
            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if f'gpio{gpio}' in line.lower() or f'gpio {gpio}' in line.lower():
                        if 'pwm' in line.lower():
                            print(f"  dmesg: {line[:100]}")
        except:
            pass

def main():
    print("=" * 60)
    print("  Diagnostic du mapping GPIO -> PWM")
    print("=" * 60)
    
    check_device_tree()
    check_pwm_mapping()
    check_gpio_pwm_connection()
    
    print("\n" + "=" * 60)
    print("  Configuration overlay attendue:")
    print("  dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4")
    print("  GPIO 18 -> PWM0_CHAN2 (func=2)")
    print("  GPIO 13 -> PWM0_CHAN1 (func2=4)")
    print("=" * 60)
    
    print("\n  Mapping actuel dans le code:")
    print("  AstraPwm1 -> canal 2 (devrait être GPIO 18)")
    print("  AstraPwm2 -> canal 1 (devrait être GPIO 13)")
    print("=" * 60)

if __name__ == "__main__":
    main()

