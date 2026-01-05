#!/usr/bin/env python3
"""
Script de test pas à pas pour diagnostiquer le GPS
"""

import sys
import os
import time
import subprocess

def print_step(step_num, description):
    """Affiche une étape du test"""
    print(f"\n{'='*60}")
    print(f"ÉTAPE {step_num}: {description}")
    print(f"{'='*60}")

def check_command(cmd, description):
    """Exécute une commande et affiche le résultat"""
    print(f"\n→ {description}")
    print(f"  Commande: {cmd}")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            print(f"  ✓ Succès")
            if result.stdout.strip():
                print(f"  Sortie:\n{result.stdout}")
            return True
        else:
            print(f"  ✗ Échec (code: {result.returncode})")
            if result.stderr.strip():
                print(f"  Erreur: {result.stderr}")
            return False
    except subprocess.TimeoutExpired:
        print(f"  ✗ Timeout")
        return False
    except Exception as e:
        print(f"  ✗ Erreur: {e}")
        return False

def test_step_1_overlay():
    """Test 1: Vérifier que l'overlay PPS est configuré"""
    print_step(1, "Vérification de l'overlay PPS dans config.txt")
    
    config_files = ["/boot/firmware/config.txt", "/boot/config.txt"]
    overlay_found = False
    
    for config_file in config_files:
        if os.path.exists(config_file):
            print(f"\n→ Fichier trouvé: {config_file}")
            with open(config_file, 'r') as f:
                content = f.read()
                if "pps-gpio" in content:
                    overlay_found = True
                    print(f"  ✓ Overlay pps-gpio trouvé")
                    for line in content.split('\n'):
                        if "pps-gpio" in line:
                            print(f"    {line.strip()}")
                else:
                    print(f"  ✗ Overlay pps-gpio NON trouvé")
    
    if not overlay_found:
        print("\n⚠️  ACTION REQUISE: Ajouter 'dtoverlay=pps-gpio,gpiopin=25' dans /boot/firmware/config.txt et redémarrer")
    
    return overlay_found

def test_step_2_pps_device():
    """Test 2: Vérifier que /dev/pps0 existe"""
    print_step(2, "Vérification du périphérique PPS /dev/pps0")
    
    if os.path.exists("/dev/pps0"):
        print("  ✓ /dev/pps0 existe")
        
        # Vérifier les permissions
        stat = os.stat("/dev/pps0")
        print(f"  Permissions: {oct(stat.st_mode)}")
        print(f"  Propriétaire: {stat.st_uid}:{stat.st_gid}")
        
        # Tester la lecture PPS
        print("\n→ Test de lecture PPS (attendre 2 secondes pour voir un signal)")
        try:
            import pps
            pps_fd = pps.open("/dev/pps0", pps.PPS_CAPTUREASSERT)
            print("  ✓ PPS ouvert avec succès")
            print("  → Attente d'un signal PPS (peut prendre jusqu'à 1 seconde)...")
            try:
                pps_data = pps.fetch(pps_fd, timeout=2.0)
                print(f"  ✓ Signal PPS reçu!")
                print(f"    Timestamp: {pps_data['assert_timestamp']}")
                pps.close(pps_fd)
                return True
            except Exception as e:
                print(f"  ⚠️  Aucun signal PPS reçu dans les 2 secondes: {e}")
                print("     (C'est normal si le GPS n'a pas encore de fix)")
                pps.close(pps_fd)
                return True  # Le périphérique existe, c'est OK
        except ImportError:
            print("  ⚠️  Module 'pps' non disponible, test avec pps-tools")
            print("  → Vérification avec ppstest...")
            result = check_command("timeout 3 ppstest /dev/pps0", "Test PPS avec ppstest")
            if not result:
                print("  → Tentative avec ppswatch...")
                # ppswatch nécessite une sortie, on va juste vérifier qu'il peut s'exécuter
                test_result = subprocess.run("timeout 1 ppswatch /dev/pps0 2>&1 | head -1", 
                                            shell=True, capture_output=True, text=True, timeout=2)
                if test_result.returncode == 0 or "timeout" in test_result.stderr.lower():
                    print("  ✓ ppswatch peut accéder à /dev/pps0")
                    return True
            return result
        except Exception as e:
            print(f"  ✗ Erreur lors de l'ouverture PPS: {e}")
            return False
    else:
        print("  ✗ /dev/pps0 n'existe pas")
        print("  ⚠️  Vérifiez que l'overlay pps-gpio est chargé (redémarrage nécessaire)")
        return False

def test_step_3_uart():
    """Test 3: Vérifier l'UART et les périphériques série"""
    print_step(3, "Vérification de l'UART et des périphériques série")
    
    # Vérifier les périphériques série disponibles
    print("\n→ Périphériques série disponibles:")
    serial_devices = []
    for dev in ["/dev/ttyAMA0", "/dev/ttyS0", "/dev/serial0"]:
        if os.path.exists(dev):
            print(f"  ✓ {dev} existe")
            serial_devices.append(dev)
        else:
            print(f"  ✗ {dev} n'existe pas")
    
    # Vérifier la configuration UART dans config.txt
    print("\n→ Configuration UART dans config.txt:")
    config_files = ["/boot/firmware/config.txt", "/boot/config.txt"]
    uart_found = False
    for config_file in config_files:
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                content = f.read()
                if "uart" in content.lower():
                    uart_found = True
                    for line in content.split('\n'):
                        if "uart" in line.lower():
                            print(f"    {line.strip()}")
    
    if not uart_found:
        print("  ⚠️  Aucune configuration UART trouvée")
    
    return len(serial_devices) > 0

def test_step_4_gpsd():
    """Test 4: Vérifier que gpsd est installé et fonctionne"""
    print_step(4, "Vérification du service gpsd")
    
    # Vérifier si gpsd est installé
    result = check_command("which gpsd", "Vérification de l'installation de gpsd")
    if not result:
        print("  ⚠️  gpsd n'est pas installé. Installer avec: sudo apt install gpsd")
        return False
    
    # Vérifier le statut du service
    result = check_command("systemctl is-active gpsd", "Statut du service gpsd")
    if result:
        status_output = subprocess.run("systemctl status gpsd --no-pager", shell=True, capture_output=True, text=True)
        if status_output.returncode == 0:
            print(f"  Sortie status:\n{status_output.stdout[:500]}")
    
    # Vérifier la configuration
    if os.path.exists("/etc/default/gpsd"):
        print("\n→ Configuration /etc/default/gpsd:")
        with open("/etc/default/gpsd", 'r') as f:
            for line in f:
                if line.strip() and not line.strip().startswith('#'):
                    print(f"    {line.strip()}")
    
    # Vérifier le socket
    if os.path.exists("/var/run/gpsd.sock"):
        print("\n  ✓ Socket gpsd trouvé: /var/run/gpsd.sock")
    else:
        print("\n  ✗ Socket gpsd non trouvé")
    
    return result

def test_step_5_gpsd_connection():
    """Test 5: Tester la connexion à gpsd"""
    print_step(5, "Test de connexion à gpsd")
    
    try:
        import gps
        from gps import gps, WATCH_ENABLE, WATCH_JSON
        
        print("\n→ Connexion à gpsd...")
        try:
            session = gps(mode=WATCH_ENABLE | WATCH_JSON)
            print("  ✓ Connexion réussie")
        except Exception as e:
            print(f"  ✗ Impossible de se connecter à gpsd: {e}")
            print("  Vérifier: sudo systemctl status gpsd")
            return False
        
        print("\n→ Attente de données GPS (10 secondes)...")
        reports_received = 0
        fix_found = False
        pps_received = False
        start_time = time.time()
        
        while time.time() - start_time < 10:
            try:
                # Utiliser un timeout pour éviter de bloquer indéfiniment
                report = session.next(timeout=1)
                reports_received += 1
                
                if report.get('class') == 'TPV':  # Time-Position-Velocity
                    mode = report.get('mode', 0)
                    print(f"  → Rapport TPV reçu (mode: {mode})")
                    if mode >= 2:
                        fix_found = True
                        print(f"  ✓ FIX {mode}D détecté!")
                        lat = report.get('lat')
                        lon = report.get('lon')
                        alt = report.get('alt')
                        gps_time = report.get('time', 'N/A')
                        print(f"    Latitude: {lat}")
                        print(f"    Longitude: {lon}")
                        print(f"    Altitude: {alt} m")
                        print(f"    Temps GPS: {gps_time}")
                        break
                    else:
                        print(f"    Mode: {mode} (No fix - attente...)")
                
                elif report.get('class') == 'PPS':
                    pps_received = True
                    print(f"  ✓ Signal PPS reçu")
                
                elif report.get('class') == 'DEVICE':
                    device = report.get('path', 'N/A')
                    print(f"  → Périphérique GPS: {device}")
                
            except Exception as e:
                error_str = str(e).lower()
                if "timeout" in error_str or "timed out" in error_str:
                    # Timeout normal, continuer
                    pass
                else:
                    print(f"  Erreur lors de la lecture: {e}")
        
        print(f"\n  Résumé:")
        print(f"    Rapports reçus: {reports_received}")
        print(f"    Signal PPS: {'✓' if pps_received else '✗'}")
        print(f"    Fix GPS: {'✓' if fix_found else '✗'}")
        
        if not fix_found:
            print("\n  ⚠️  Aucun fix GPS détecté dans les 10 secondes")
            print("     Causes possibles:")
            print("     - GPS pas encore synchronisé (attendre 1-5 minutes)")
            print("     - Antenne GPS non connectée ou masquée")
            print("     - GPS non alimenté")
            print("     - Mauvais câblage série")
        
        return True
        
    except ImportError:
        print("  ✗ Module 'gps' non disponible")
        print("  Installer avec: sudo apt install python3-gps")
        return False
    except Exception as e:
        print(f"  ✗ Erreur de connexion: {e}")
        return False

def test_step_6_gps_device():
    """Test 6: Vérifier le périphérique GPS physique"""
    print_step(6, "Vérification du périphérique GPS physique")
    
    # Vérifier les périphériques USB
    print("\n→ Périphériques USB:")
    result = check_command("lsusb | grep -i 'gps\|ublox\|serial'", "Recherche de périphériques GPS USB")
    
    # Vérifier les périphériques série
    print("\n→ Périphériques série:")
    result = check_command("dmesg | grep -i 'tty\|uart\|serial' | tail -10", "Derniers messages série dans dmesg")
    
    # Vérifier les permissions
    print("\n→ Permissions des périphériques série:")
    for dev in ["/dev/ttyAMA0", "/dev/ttyS0", "/dev/serial0"]:
        if os.path.exists(dev):
            stat = os.stat(dev)
            print(f"  {dev}: {oct(stat.st_mode)} (uid:{stat.st_uid}, gid:{stat.st_gid})")
    
    return True

def test_step_7_chrony():
    """Test 7: Vérifier la configuration Chrony"""
    print_step(7, "Vérification de la configuration Chrony")
    
    # Vérifier si chrony est installé
    result = check_command("which chronyd", "Vérification de l'installation de chronyd")
    
    if result:
        # Vérifier le statut
        check_command("systemctl is-active chronyd", "Statut du service chronyd")
        
        # Vérifier la configuration PPS
        if os.path.exists("/etc/chrony/conf.d/chronyastralim.conf"):
            print("\n→ Configuration Chrony PPS:")
            with open("/etc/chrony/conf.d/chronyastralim.conf", 'r') as f:
                for line in f:
                    if line.strip() and not line.strip().startswith('#'):
                        print(f"    {line.strip()}")
        else:
            print("\n  ⚠️  Fichier de configuration Chrony non trouvé")
            print("     Attendu: /etc/chrony/conf.d/chronyastralim.conf")
        
        # Vérifier les sources
        print("\n→ Sources NTP de Chrony:")
        check_command("chronyc sources", "Liste des sources")
        check_command("chronyc tracking", "État de synchronisation")
    
    return True

def test_step_8_astra_gps():
    """Test 8: Tester directement la classe AstraGps"""
    print_step(8, "Test de la classe AstraGps (comme dans l'application)")
    
    try:
        # Ajouter le répertoire parent au path pour l'import (tests/ -> pythonDrivers/)
        parent_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if parent_dir not in sys.path:
            sys.path.insert(0, parent_dir)
        from AstraGps import AstraGps
        
        print("\n→ Création d'une instance AstraGps...")
        gps_instance = AstraGps.get_instance()
        print("  ✓ Instance créée")
        
        print("\n→ Attente de 5 secondes pour collecter des données...")
        time.sleep(5)
        
        # Lire les données
        sync_state = gps_instance.gpsSyncState()
        fix_mode = gps_instance.fixMode
        lat, lon, alt = gps_instance.gpsGetStrPosition()
        gps_time = gps_instance.gpsTimeStamp()
        pps_count = gps_instance.gpsCountPPS()
        
        print(f"\n  État de synchronisation: {sync_state}")
        print(f"  Mode fix: {fix_mode}")
        print(f"  Position: Lat={lat}, Lon={lon}, Alt={alt}")
        print(f"  Temps GPS: {gps_time}")
        print(f"  Compteur PPS: {pps_count}")
        
        if fix_mode >= 2:
            print("\n  ✓ Fix GPS détecté par AstraGps!")
            return True
        else:
            print("\n  ⚠️  Pas de fix GPS (normal si le GPS vient d'être allumé)")
            return True  # La classe fonctionne, même sans fix
        
    except ImportError as e:
        print(f"  ✗ Erreur d'import: {e}")
        print("  Vérifier que AstraGps.py est dans le même répertoire")
        return False
    except Exception as e:
        print(f"  ✗ Erreur: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    print("="*60)
    print("  TEST GPS PAS À PAS")
    print("="*60)
    print("\nCe script va tester chaque composant du système GPS")
    print("pour identifier où se situe le problème.\n")
    
    results = {}
    
    # Exécuter tous les tests
    results['overlay'] = test_step_1_overlay()
    results['pps_device'] = test_step_2_pps_device()
    results['uart'] = test_step_3_uart()
    results['gpsd'] = test_step_4_gpsd()
    results['gpsd_connection'] = test_step_5_gpsd_connection()
    results['gps_device'] = test_step_6_gps_device()
    results['chrony'] = test_step_7_chrony()
    results['astra_gps'] = test_step_8_astra_gps()
    
    # Résumé
    print("\n" + "="*60)
    print("  RÉSUMÉ DES TESTS")
    print("="*60)
    
    for test_name, result in results.items():
        status = "✓" if result else "✗"
        print(f"{status} {test_name}")
    
    print("\n" + "="*60)
    print("  DIAGNOSTIC")
    print("="*60)
    
    if not results['overlay']:
        print("\n⚠️  PROBLÈME 1: Overlay PPS non configuré")
        print("   Solution: Ajouter 'dtoverlay=pps-gpio,gpiopin=25' dans /boot/firmware/config.txt")
        print("   Puis redémarrer: sudo reboot")
    
    if not results['pps_device']:
        print("\n⚠️  PROBLÈME 2: Périphérique PPS /dev/pps0 non disponible")
        print("   Solution: Vérifier l'overlay et redémarrer")
    
    if not results['gpsd']:
        print("\n⚠️  PROBLÈME 3: Service gpsd non actif")
        print("   Solution: sudo systemctl start gpsd")
        print("   Vérifier: sudo systemctl status gpsd")
    
    if not results['gpsd_connection']:
        print("\n⚠️  PROBLÈME 4: Impossible de se connecter à gpsd")
        print("   Solution: Vérifier que gpsd est démarré et écoute sur le bon socket")
        print("   Vérifier: sudo systemctl restart gpsd")
    
    if results['gpsd_connection'] and not any([results['overlay'], results['pps_device']]):
        print("\n⚠️  PROBLÈME 5: GPS connecté mais pas de fix")
        print("   Causes possibles:")
        print("   - GPS pas encore synchronisé (attendre 1-5 minutes)")
        print("   - Antenne GPS non connectée ou masquée")
        print("   - GPS non alimenté")
        print("   - Mauvais câblage")
    
    print("\n" + "="*60)

if __name__ == "__main__":
    main()

