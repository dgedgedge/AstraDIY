#!/usr/bin/env python3
"""
Script de diagnostic pour vérifier les mesures INA219
Utile pour déboguer les problèmes de mesure de courant avec PWM
"""
import sys
import os
import time
# Ajouter le répertoire parent au path (tests/ -> pythonDrivers/)
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)

from AstraIna import AstraIna

def main():
    print("=== Diagnostic INA219 pour bandes chauffantes ===\n")
    
    # Tester les deux canaux PWM
    for name in ["AstraPwm1", "AstraPwm2"]:
        print(f"\n--- {name} ---")
        try:
            ina = AstraIna(name=name)
            time.sleep(0.5)  # Attendre que les mesures soient prises
            
            # Obtenir les informations de diagnostic
            diag = ina.getDiagnosticInfo()
            
            if "error" in diag:
                print(f"ERREUR: {diag['error']}")
                continue
            
            print(f"Ping OK: {diag['ping']}")
            print(f"Configuré: {diag['configured']}")
            print(f"Configuration envoyée: {diag['configuration_sent']}")
            print(f"Voltage Range: {diag['voltage_range']} (0=16V, 1=32V)")
            print(f"Gain: {diag['gain']}")
            print(f"Shunt résistance: {diag['shunt_ohms']}Ω")
            print(f"\nValeurs mesurées:")
            print(f"  Tension bus: {diag['bus_voltage_v']:.3f}V")
            print(f"  Tension shunt: {diag['shunt_voltage_mv']:.3f}mV")
            print(f"  Courant mesuré: {diag['current_ma']:.3f}mA ({diag['current_ma']/1000:.3f}A)")
            print(f"  Courant théorique (V_shunt/R_shunt): {diag['current_theoretical_ma']:.3f}mA ({diag['current_theoretical_ma']/1000:.3f}A)")
            print(f"  Puissance: {diag['power_mw']:.3f}mW ({diag['power_mw']/1000:.3f}W)")
            print(f"  Overflow courant: {diag['current_overflow']}")
            
            # Calculer la différence
            diff = abs(diag['current_ma'] - diag['current_theoretical_ma'])
            print(f"\nDifférence courant mesuré vs théorique: {diff:.3f}mA")
            
            if diff > 10:  # Si différence > 10mA
                print("⚠️  ATTENTION: Grande différence entre courant mesuré et théorique!")
                print("   Cela peut indiquer un problème de calibration ou de mesure.")
            
            # Vérifier si le courant est cohérent avec la puissance
            if diag['bus_voltage_v'] > 0.1:  # Éviter division par zéro
                current_from_power = (diag['power_mw'] / 1000.0) / diag['bus_voltage_v'] * 1000.0  # en mA
                print(f"  Courant calculé depuis puissance: {current_from_power:.3f}mA")
                print(f"  Différence: {abs(diag['current_ma'] - current_from_power):.3f}mA")
            
        except Exception as e:
            print(f"ERREUR lors de la création de {name}: {e}")
            import traceback
            traceback.print_exc()
    
    print("\n=== Fin du diagnostic ===")
    print("\nNote: Avec un signal PWM, le courant mesuré devrait être le courant moyen.")
    print("Si la bande chauffante consomme vraiment du courant mais que la mesure")
    print("montre 0.01A, vérifiez:")
    print("1. La configuration du shunt (0.01Ω)")
    print("2. La calibration du INA219")
    print("3. Si le signal PWM n'interfère pas avec la mesure")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrompu par l'utilisateur")
        AstraIna.exitAll()
        sys.exit(0)

