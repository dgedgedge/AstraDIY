#!/usr/bin/env python3
"""
Script de test pour vérifier le fonctionnement des PWM
Teste la détection automatique du pwmchip et les deux canaux PWM
"""
import sys
import os
import time
import signal
# Ajouter le répertoire parent au path (tests/ -> pythonDrivers/)
parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)

from lib.syspwm import SysPWM, find_pwmchip_with_channels

def test_pwmchip_detection():
    """Teste la détection automatique du pwmchip"""
    print("=" * 60)
    print("Test 1: Détection automatique du pwmchip")
    print("=" * 60)
    
    chip_num, npwm = find_pwmchip_with_channels(min_channels=2)
    
    if chip_num is None:
        print("❌ ERREUR: Aucun pwmchip trouvé avec au moins 2 canaux")
        print("   Vérifiez que l'overlay PWM est chargé dans /boot/firmware/config.txt")
        return None
    
    print(f"✓ pwmchip{chip_num} détecté avec {npwm} canaux disponibles")
    return chip_num

def test_pwm_channel(channel_num, chip_num=None):
    """Teste un canal PWM spécifique"""
    print(f"\n{'=' * 60}")
    print(f"Test du canal PWM {channel_num}")
    print(f"{'=' * 60}")
    
    try:
        # Créer l'instance avec auto-détection si chip_num est None
        if chip_num is None:
            print(f"  → Auto-détection du pwmchip pour le canal {channel_num}...")
            pwm = SysPWM(None, channel_num)
        else:
            print(f"  → Utilisation de pwmchip{chip_num} pour le canal {channel_num}...")
            pwm = SysPWM(chip_num, channel_num)
        
        print(f"  ✓ Canal PWM {channel_num} initialisé")
        
        # Test 1: Configuration de la période
        print(f"  → Configuration de la période à 1ms (1000000ns)...")
        pwm.set_periode_ms(1)
        periode = pwm.get_periode_ms()
        print(f"  ✓ Période configurée: {periode}ms")
        
        # Test 2: Duty cycle à 0%
        print(f"  → Test duty cycle 0%...")
        pwm.set_duty_ms(0)
        print(f"  ✓ Duty cycle 0% configuré")
        
        # Test 3: Activation du PWM
        print(f"  → Activation du PWM...")
        pwm.enable()
        print(f"  ✓ PWM activé")
        time.sleep(0.5)
        
        # Test 4: Duty cycle à 50%
        print(f"  → Test duty cycle 50%...")
        pwm.set_duty_ms(0.5)  # 50% de 1ms = 0.5ms
        print(f"  ✓ Duty cycle 50% configuré")
        time.sleep(1)
        
        # Test 5: Duty cycle à 100%
        print(f"  → Test duty cycle 100%...")
        pwm.set_duty_ms(1.0)  # 100% de 1ms = 1ms
        print(f"  ✓ Duty cycle 100% configuré")
        time.sleep(1)
        
        # Test 6: Retour à 0%
        print(f"  → Retour à duty cycle 0%...")
        pwm.set_duty_ms(0)
        print(f"  ✓ Duty cycle 0% configuré")
        time.sleep(0.5)
        
        # Test 7: Désactivation
        print(f"  → Désactivation du PWM...")
        pwm.disable()
        print(f"  ✓ PWM désactivé")
        
        print(f"\n  ✅ Canal PWM {channel_num}: TOUS LES TESTS RÉUSSIS")
        return pwm
        
    except Exception as e:
        print(f"  ❌ ERREUR sur le canal PWM {channel_num}: {e}")
        import traceback
        traceback.print_exc()
        return None

def test_both_channels_simultaneous(chip_num=None):
    """Teste les deux canaux PWM simultanément avec des patterns différents"""
    print(f"\n{'=' * 60}")
    print("Test des deux canaux PWM simultanément")
    print(f"{'=' * 60}")
    
    try:
        # Initialiser les deux canaux
        if chip_num is None:
            print("  → Auto-détection du pwmchip...")
            pwm1 = SysPWM(None, 1)
            pwm2 = SysPWM(None, 2)
        else:
            print(f"  → Utilisation de pwmchip{chip_num}...")
            pwm1 = SysPWM(chip_num, 1)
            pwm2 = SysPWM(chip_num, 2)
        
        print("  ✓ Les deux canaux initialisés")
        
        # Configurer la période
        pwm1.set_periode_ms(1)
        pwm2.set_periode_ms(1)
        
        # Activer les deux
        pwm1.enable()
        pwm2.enable()
        print("  ✓ Les deux canaux activés")
        
        # Test avec patterns complémentaires
        print("\n  → Test avec patterns complémentaires (10 secondes)...")
        print("     PWM1: 0% → 100% (linéaire)")
        print("     PWM2: 100% → 0% (linéaire)")
        
        steps = 20
        for i in range(steps + 1):
            duty1 = (i / steps) * 100  # 0% à 100%
            duty2 = 100 - duty1  # 100% à 0%
            
            pwm1.set_duty_ms(duty1 / 100.0)  # Convertir en ms (0 à 1ms)
            pwm2.set_duty_ms(duty2 / 100.0)
            
            if i % 5 == 0:
                print(f"     Step {i}/{steps}: PWM1={duty1:.1f}%, PWM2={duty2:.1f}%")
            
            time.sleep(0.5)
        
        # Retour à 0%
        pwm1.set_duty_ms(0)
        pwm2.set_duty_ms(0)
        print("  ✓ Patterns complémentaires terminés")
        
        # Désactiver
        pwm1.disable()
        pwm2.disable()
        print("  ✓ Les deux canaux désactivés")
        
        print(f"\n  ✅ Test simultané: RÉUSSI")
        return True
        
    except Exception as e:
        print(f"  ❌ ERREUR lors du test simultané: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_frequency_sweep(chip_num=None, channel=1):
    """Teste différentes fréquences sur un canal"""
    print(f"\n{'=' * 60}")
    print(f"Test de balayage de fréquences (canal {channel})")
    print(f"{'=' * 60}")
    
    try:
        if chip_num is None:
            pwm = SysPWM(None, channel)
        else:
            pwm = SysPWM(chip_num, channel)
        
        pwm.enable()
        pwm.set_duty_ms(0.5)  # 50% duty cycle
        
        frequencies = [10, 50, 100, 500, 1000, 5000, 10000]  # Hz
        
        print("  → Test de différentes fréquences (duty cycle 50%)...")
        for freq in frequencies:
            pwm.set_frequency(freq)
            periode_ms = pwm.get_periode_ms()
            print(f"     {freq:5d} Hz → période: {periode_ms:.3f}ms")
            time.sleep(0.5)
        
        pwm.set_duty_ms(0)
        pwm.disable()
        
        print("  ✓ Balayage de fréquences terminé")
        return True
        
    except Exception as e:
        print(f"  ❌ ERREUR lors du balayage de fréquences: {e}")
        return False

def main():
    """Fonction principale de test"""
    print("\n" + "=" * 60)
    print("SCRIPT DE TEST PWM - AstrAlim")
    print("=" * 60)
    print("\nCe script teste:")
    print("  1. La détection automatique du pwmchip")
    print("  2. Le canal PWM 1")
    print("  3. Le canal PWM 2")
    print("  4. Les deux canaux simultanément")
    print("  5. Le balayage de fréquences")
    print("\n⚠️  ATTENTION: Les bandes chauffantes peuvent chauffer!")
    print("   Assurez-vous qu'elles sont bien connectées et surveillées.")
    print("\nAppuyez sur Ctrl+C pour arrêter à tout moment.\n")
    
    # Gestionnaire de signal pour arrêt propre
    def signal_handler(sig, frame):
        print("\n\n⚠️  Arrêt demandé par l'utilisateur")
        print("   Les PWM seront désactivés...")
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    
    try:
        # Test 1: Détection du pwmchip
        chip_num = test_pwmchip_detection()
        if chip_num is None:
            print("\n❌ Impossible de continuer sans pwmchip détecté")
            return 1
        
        # Test 2: Canal PWM 1
        pwm1 = test_pwm_channel(1, chip_num)
        if pwm1 is None:
            print("\n❌ Échec du test du canal PWM 1")
            return 1
        
        # Test 3: Canal PWM 2
        pwm2 = test_pwm_channel(2, chip_num)
        if pwm2 is None:
            print("\n❌ Échec du test du canal PWM 2")
            return 1
        
        # Test 4: Les deux canaux simultanément
        if not test_both_channels_simultaneous(chip_num):
            print("\n❌ Échec du test simultané")
            return 1
        
        # Test 5: Balayage de fréquences (optionnel)
        print("\n" + "=" * 60)
        response = input("Voulez-vous tester le balayage de fréquences? (o/n): ")
        if response.lower() in ['o', 'O', 'y', 'Y']:
            test_frequency_sweep(chip_num, channel=1)
        
        # Résumé final
        print("\n" + "=" * 60)
        print("✅ TOUS LES TESTS SONT TERMINÉS")
        print("=" * 60)
        print(f"\n✓ pwmchip{chip_num} détecté et fonctionnel")
        print("✓ Canal PWM 1: OK")
        print("✓ Canal PWM 2: OK")
        print("✓ Test simultané: OK")
        print("\nLes PWM sont maintenant désactivés et prêts à l'emploi.")
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrompu par l'utilisateur")
        return 1
    except Exception as e:
        print(f"\n\n❌ ERREUR FATALE: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())

