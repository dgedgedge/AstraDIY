#!/usr/bin/env python3
"""
Script pour scanner toutes les adresses INA219 sur le bus I2C
et afficher les valeurs de tension, courant et puissance pour chaque INA détecté.
"""
import sys
import time
from lib.ina219 import INA219, I2CError

def scan_ina219_addresses(busnum=1, shunt_ohms=0.01, max_expected_amps=6):
    """
    Scanne toutes les adresses INA219 possibles (0x40-0x4F)
    et retourne les INA qui répondent avec leurs valeurs.
    """
    results = {}
    addresses_to_scan = range(0x40, 0x50)  # 0x40 à 0x4F
    
    print("=" * 70)
    print("  SCAN DES INA219 SUR LE BUS I2C")
    print("=" * 70)
    print(f"Bus I2C: {busnum}")
    print(f"Adresses à scanner: 0x40 - 0x4F (64 adresses)")
    print("-" * 70)
    
    for address in addresses_to_scan:
        try:
            print(f"Test de l'adresse 0x{address:02x}...", end=" ", flush=True)
            
            # Créer une instance INA219 pour tester
            ina = INA219(
                shunt_ohms=shunt_ohms,
                max_expected_amps=max_expected_amps,
                busnum=busnum,
                address=address,
                log_level=0  # Pas de logs
            )
            
            # Tester si l'INA répond
            if ina.ping():
                print("✓ DÉTECTÉ")
                
                # Configurer l'INA
                ina.configure(
                    voltage_range=INA219.RANGE_32V,
                    gain=INA219.GAIN_AUTO,
                    bus_adc=INA219.ADC_128SAMP,
                    shunt_adc=INA219.ADC_128SAMP
                )
                
                # Attendre un peu pour que la configuration soit prise en compte
                time.sleep(0.1)
                
                # Lire les valeurs
                try:
                    bus_voltage = ina.voltage()
                    shunt_voltage = ina.shunt_voltage()
                    current = ina.current()  # en milliamps
                    power = ina.power()  # en milliwatts
                    overflow = ina.current_overflow()
                    
                    results[address] = {
                        "bus_voltage": bus_voltage,
                        "shunt_voltage": shunt_voltage,
                        "current": current,
                        "power": power,
                        "overflow": overflow,
                        "status": "OK"
                    }
                    
                    print(f"    Tension bus: {bus_voltage:.3f}V")
                    print(f"    Tension shunt: {shunt_voltage:.3f}mV")
                    print(f"    Courant: {current:.3f}mA ({current/1000:.3f}A)")
                    print(f"    Puissance: {power:.3f}mW ({power/1000:.3f}W)")
                    if overflow:
                        print(f"    ⚠️  OVERFLOW de courant détecté!")
                    
                except Exception as e:
                    print(f"    ⚠️  Erreur lors de la lecture: {e}")
                    results[address] = {
                        "status": "ERROR",
                        "error": str(e)
                    }
            else:
                print("✗ Non détecté")
                
        except I2CError as e:
            print(f"✗ Erreur I2C: {e}")
        except Exception as e:
            print(f"✗ Erreur: {e}")
    
    return results

def main():
    print("\nDémarrage du scan INA219...")
    print("Assurez-vous que la puissance de RCA1 est activée à 40% avant de continuer.")
    print("\nAppuyez sur Entrée pour commencer le scan...")
    input()
    
    print("\nScan en cours...\n")
    results = scan_ina219_addresses(busnum=1, shunt_ohms=0.01, max_expected_amps=6)
    
    print("\n" + "=" * 70)
    print("  RÉSUMÉ DES RÉSULTATS")
    print("=" * 70)
    
    if not results:
        print("Aucun INA219 détecté sur le bus I2C.")
    else:
        print(f"{len(results)} INA219 détecté(s):\n")
        for address, data in sorted(results.items()):
            if data.get("status") == "OK":
                print(f"0x{address:02x} ({address:3d}):")
                print(f"  Tension bus: {data['bus_voltage']:.3f}V")
                print(f"  Tension shunt: {data['shunt_voltage']:.3f}mV")
                print(f"  Courant: {data['current']:.3f}mA ({data['current']/1000:.3f}A)")
                print(f"  Puissance: {data['power']:.3f}mW ({data['power']/1000:.3f}W)")
                if data.get('overflow'):
                    print(f"  ⚠️  OVERFLOW de courant!")
                print()
            else:
                print(f"0x{address:02x} ({address:3d}): ERREUR - {data.get('error', 'Unknown')}")
                print()
    
    # Comparaison avec la configuration attendue
    print("=" * 70)
    print("  COMPARAISON AVEC LA CONFIGURATION ATTENDUE")
    print("=" * 70)
    expected_addresses = {
        0x41: "AstraDc1",
        0x44: "AstraDc2",
        0x46: "AstraDc3",
        0x49: "AstraPwm1",
        0x4d: "AstraPwm2"
    }
    
    for address, name in expected_addresses.items():
        if address in results:
            data = results[address]
            if data.get("status") == "OK":
                print(f"✓ {name} (0x{address:02x}): OK - V={data['bus_voltage']:.3f}V, I={data['current']:.3f}mA")
            else:
                print(f"✗ {name} (0x{address:02x}): ERREUR - {data.get('error', 'Unknown')}")
        else:
            print(f"✗ {name} (0x{address:02x}): NON DÉTECTÉ")
    
    print("\n" + "=" * 70)
    print("Scan terminé.")
    print("=" * 70)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nScan interrompu par l'utilisateur.")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nErreur fatale: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

