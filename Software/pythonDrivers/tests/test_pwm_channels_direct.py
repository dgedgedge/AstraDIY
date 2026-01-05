#!/usr/bin/env python3
"""
Script pour tester directement chaque canal PWM et vérifier quel GPIO est activé
en testant avec un multimètre ou en lisant les valeurs INA
"""

import sys
import os
# Ajouter le répertoire parent au path (tests/ -> pythonDrivers/)
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)

from lib.syspwm import SysPWM, find_pwmchip_with_channels
import time

def test_channel_with_ina(channel, ina_address, expected_gpio):
    """Teste un canal PWM et vérifie si l'INA correspondant détecte de l'activité"""
    print(f"\n{'='*60}")
    print(f"Test du canal {channel} (devrait être GPIO {expected_gpio})")
    print(f"{'='*60}")
    
    try:
        chip_num, npwm = find_pwmchip_with_channels(min_channels=2)
        if chip_num is None:
            print("❌ Aucun pwmchip trouvé")
            return False
        
        pwm = SysPWM(chip_num, channel)
        print(f"✓ Canal {channel} initialisé sur pwmchip{chip_num}")
        
        # Configurer PWM
        pwm.set_periode_ms(1)  # 1ms période
        pwm.set_duty_ms(0.5)   # 50% duty cycle
        pwm.enable()
        print(f"✓ Canal {channel} activé à 50% (0.5ms duty sur 1ms période)")
        
        # Lire l'INA
        try:
            from lib.ina219 import INA219
            ina = INA219(shunt_ohms=0.01, max_expected_amps=6, busnum=1, address=ina_address)
            ina.configure(bus_adc=INA219.ADC_128SAMP, shunt_adc=INA219.ADC_128SAMP)
            time.sleep(0.2)  # Attendre que l'INA se stabilise
            
            voltage = ina.voltage()
            current = ina.current()
            power = ina.power()
            
            print(f"\nLecture INA à l'adresse 0x{ina_address:02x}:")
            print(f"  Tension: {voltage:.3f}V")
            print(f"  Courant: {current:.3f}mA ({current/1000:.3f}A)")
            print(f"  Puissance: {power:.3f}mW")
            
            # Si on détecte une tension significative (>0.1V), le canal est correct
            if voltage > 0.1:
                print(f"\n✅ SUCCÈS: Canal {channel} alimente bien l'INA 0x{ina_address:02x} (GPIO {expected_gpio})")
                pwm.disable()
                return True
            else:
                print(f"\n❌ ÉCHEC: Canal {channel} n'alimente pas l'INA 0x{ina_address:02x} (tension trop faible)")
                pwm.disable()
                return False
                
        except Exception as e:
            print(f"⚠️  Erreur lors de la lecture INA: {e}")
            print(f"   Veuillez vérifier manuellement avec un multimètre sur GPIO {expected_gpio}")
            pwm.disable()
            return False
            
    except Exception as e:
        print(f"❌ Erreur lors de l'initialisation du canal {channel}: {e}")
        return False

def main():
    print("="*60)
    print("  Test direct des canaux PWM -> GPIO")
    print("="*60)
    print("\nCe script teste chaque canal PWM et vérifie quel INA détecte de l'activité.")
    print("Configuration attendue:")
    print("  - GPIO 18 -> Canal X -> INA 0x49 (AstraPwm1)")
    print("  - GPIO 13 -> Canal Y -> INA 0x4d (AstraPwm2)")
    print("\n" + "="*60)
    
    # Tester les différentes combinaisons possibles
    test_cases = [
        (0, 0x49, 18, "Canal 0 -> GPIO 18 -> INA 0x49"),
        (1, 0x4d, 13, "Canal 1 -> GPIO 13 -> INA 0x4d"),
        (2, 0x49, 18, "Canal 2 -> GPIO 18 -> INA 0x49"),
        (3, 0x49, 18, "Canal 3 -> GPIO 18 -> INA 0x49"),
        (1, 0x49, 18, "Canal 1 -> GPIO 18 -> INA 0x49"),
        (2, 0x4d, 13, "Canal 2 -> GPIO 13 -> INA 0x4d"),
    ]
    
    results = {}
    for channel, ina_addr, expected_gpio, description in test_cases:
        print(f"\n\n{'#'*60}")
        print(f"Test: {description}")
        print(f"{'#'*60}")
        result = test_channel_with_ina(channel, ina_addr, expected_gpio)
        results[(channel, ina_addr)] = result
        time.sleep(1)  # Pause entre les tests
    
    # Résumé
    print("\n\n" + "="*60)
    print("  RÉSUMÉ DES TESTS")
    print("="*60)
    for (channel, ina_addr), success in results.items():
        status = "✅" if success else "❌"
        print(f"{status} Canal {channel} -> INA 0x{ina_addr:02x}: {'FONCTIONNE' if success else 'NE FONCTIONNE PAS'}")
    
    print("\n" + "="*60)
    print("  Configuration recommandée:")
    working_mappings = [k for k, v in results.items() if v]
    if working_mappings:
        for (channel, ina_addr) in working_mappings:
            if ina_addr == 0x49:
                print(f"  AstraPwm1 (GPIO 18, INA 0x49) -> Canal {channel}")
            elif ina_addr == 0x4d:
                print(f"  AstraPwm2 (GPIO 13, INA 0x4d) -> Canal {channel}")
    else:
        print("  Aucun mapping fonctionnel trouvé. Vérifiez le câblage matériel.")
    print("="*60)

if __name__ == "__main__":
    main()

