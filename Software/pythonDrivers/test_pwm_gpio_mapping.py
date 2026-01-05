#!/usr/bin/env python3
"""
Script pour tester le mapping réel des canaux PWM vers les GPIO
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from lib.syspwm import SysPWM, find_pwmchip_with_channels
import time

def test_channel_mapping():
    """Teste chaque canal PWM et affiche les informations de mapping"""
    print("=" * 60)
    print("  Test du mapping PWM -> GPIO")
    print("=" * 60)
    
    # Détecter le pwmchip
    chip_num, npwm = find_pwmchip_with_channels(min_channels=2)
    if chip_num is None:
        print("❌ Aucun pwmchip trouvé")
        return
    
    print(f"\n✓ pwmchip{chip_num} détecté avec {npwm} canaux")
    
    # Tester chaque canal disponible
    for channel in range(min(npwm, 4)):  # Tester jusqu'à 4 canaux max
        print(f"\n--- Test du canal {channel} ---")
        try:
            pwm = SysPWM(chip_num, channel)
            print(f"✓ Canal {channel} exporté avec succès")
            
            # Lire les informations du périphérique
            pwmdir = pwm.pwmdir
            print(f"  Chemin: {pwmdir}")
            
            # Vérifier si le périphérique existe
            if os.path.exists(pwmdir):
                # Lire uevent
                uevent_path = os.path.join(pwmdir, "uevent")
                if os.path.exists(uevent_path):
                    with open(uevent_path, 'r') as f:
                        uevent = f.read()
                        print(f"  uevent: {uevent[:200]}")
                
                # Lire le lien symbolique device
                device_path = os.path.join(pwmdir, "device")
                if os.path.exists(device_path) and os.path.islink(device_path):
                    device_real = os.readlink(device_path)
                    print(f"  device: {device_real}")
                    
                    # Chercher GPIO dans le chemin
                    if "gpio" in device_real.lower():
                        import re
                        match = re.search(r'gpio[_-]?(\d+)', device_real, re.IGNORECASE)
                        if match:
                            gpio_num = int(match.group(1))
                            print(f"  → GPIO détecté: {gpio_num}")
                
                # Lire of_node si disponible
                of_node_path = os.path.join(pwmdir, "device", "of_node")
                if os.path.exists(of_node_path):
                    print(f"  of_node existe: {of_node_path}")
                    # Lire compatible
                    compatible_path = os.path.join(of_node_path, "compatible")
                    if os.path.exists(compatible_path):
                        with open(compatible_path, 'r') as f:
                            compatible = f.read().strip()
                            print(f"  compatible: {compatible}")
            
            # Tester l'activation du canal
            pwm.set_periode_ms(1)
            pwm.set_duty_ms(0.5)  # 50% duty cycle
            pwm.enable()
            print(f"  ✓ Canal {channel} activé (50% duty cycle, 1ms période)")
            
            time.sleep(0.5)
            
            # Désactiver
            pwm.disable()
            print(f"  ✓ Canal {channel} désactivé")
            
        except Exception as e:
            print(f"  ❌ Erreur avec le canal {channel}: {e}")
    
    print("\n" + "=" * 60)
    print("  Configuration attendue:")
    print("  dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4")
    print("  GPIO 18 -> PWM0_CHAN2")
    print("  GPIO 13 -> PWM0_CHAN1")
    print("=" * 60)
    print("\n  Mapping actuel dans le code:")
    print("  AstraPwm1 -> canal 2 (devrait être GPIO 18)")
    print("  AstraPwm2 -> canal 1 (devrait être GPIO 13)")
    print("=" * 60)

if __name__ == "__main__":
    test_channel_mapping()

