#!/usr/bin/env python3
# GPIO used PA17
import threading
from pathlib import Path
import time
import atexit
import json
from typing import Any, Optional

from lib.astra_step_pwm_actor import AstraStepPwmActor
from lib.astra_1wire_temp_fetcher import Astra1WireTempFetcher
from lib.astra_bme_fetcher import AstraBmeFetcher

def _get_debug_log_path() -> str:
    """Retourne le chemin du fichier de log de debug."""
    # Utiliser /tmp pour éviter les problèmes de permissions
    return "/tmp/astradiy_debug.log"

def _write_debug_log(
    session_id: str,
    run_id: str,
    hypothesis_id: str,
    location: str,
    message: str,
    data: dict[str, Any],
) -> None:
    """Écrit une ligne JSON de debug dans le fichier de log temporaire."""
    try:
        log_path = _get_debug_log_path()
        Path(log_path).parent.mkdir(parents=True, exist_ok=True)
        with open(log_path, "a") as f:
            f.write(json.dumps({"sessionId":session_id,"runId":run_id,"hypothesisId":hypothesis_id,"location":location,"message":message,"data":data,"timestamp":int(time.time()*1000)})+"\n")
    except Exception as e:
        print(f"[DEBUG LOG ERROR] {e}")


class RaspberryPiModel:
    """Détecte le modèle de Raspberry Pi à partir du device tree."""

    def __init__(self) -> None:
        self.compatible_strings: list[str] = self._read_compatible_strings()

    def _read_compatible_strings(self) -> list[str]:
        """Lit la liste des chaînes `compatible` exposées par le noyau."""
        try:
            with open('/proc/device-tree/compatible', 'r') as file:
                return file.read().split('\x00')
        except FileNotFoundError:
            return []

    def getModelNumber(self) -> str:
        """Retourne le numéro de modèle Raspberry Pi détecté."""
        for string in self.compatible_strings:
            if string.startswith('raspberrypi,'):
                # Extract the model number from the string
                model_number = string.split(',')[1].split('-')[0]
                return model_number
        return "Unknown Model"
    
    def getPi(self) -> str:
        """Retourne une clé de plateforme du type `pi4` ou `pi5`."""

        return f"pi{self.getModelNumber()}"
    
class AstraPwm():
    """Pilote une sortie chauffante PWM et son asservissement thermique.

    Le rapport cyclique piloté ici représente directement la puissance moyenne
    envoyée à la charge chauffante. Avec une alimentation 12 V et une puissance
    maximale estimée à 36 W, un rapport de 50 % correspond donc à environ 18 W
    moyens dissipés dans la résistance.
    """

    ROSEEUNAVAIL=AstraBmeFetcher.ROSEEUNAVAIL
    TEMPUNAVAIL=AstraBmeFetcher.TEMPUNAVAIL
    astraGpioSet = { 
                "AstraPwm1": {
                    "pi5": { "gpio":18 },# AstraPwm1 -> GPIO 18 (INA 0x49)
                    "pi4": { "gpio":18 },
                              },
                "AstraPwm2": {
                    "pi5": { "gpio":13 },# AstraPwm2 -> GPIO 13 (INA 0x4d)
                    "pi4": { "gpio":13 },
                }
    }

    def __init__(self, name: str, MinTemp: float = 0, MaxTemp: float = 20) -> None:
        self.name: str = name
        self.piModel: str = RaspberryPiModel().getPi()
        if name in self.astraGpioSet :
            if self.piModel in self.astraGpioSet[self.name]:
                self.inacaract: dict[str, int] = self.astraGpioSet[self.name][self.piModel]
            else:
                raise Exception(f"Not compatible pi model : {self.piModel}")
        else:
            raise Exception("Unkown AstraGpio")


        self.ratio: float = 0
        self.period_ms: int = 100
        gpioNumber = self.inacaract["gpio"]
        # #region agent log
        _write_debug_log("debug-session", "init", "A,B,D", "AstraPwm.py:__init__", "Before AstraStepPwmActor creation", {"name":self.name,"gpio":gpioNumber,"piModel":self.piModel})
        # #endregion
        self.pwmActor = AstraStepPwmActor(name=self.name, gpio=gpioNumber, stepPercent=self.ratio)
        print(f"[DEBUG AstraPwm.__init__] {self.name}: Step PWM actor initialisé sur GPIO {gpioNumber}, ratio={self.ratio}%")
        # #region agent log
        _write_debug_log("debug-session", "init", "A,B,D", "AstraPwm.py:__init__", "After AstraStepPwmActor creation", {"name":self.name,"gpio":gpioNumber})
        # #endregion
        atexit.register(self.pwmActor.close)

        self.oneWireFetcher = Astra1WireTempFetcher.get_instance()
        self.bmeFetcher = AstraBmeFetcher.get_instance()
        self.tempname: Optional[str] = self.get_default_temp()
        print(f"[DEBUG AstraPwm.__init__] {self.name}: tempname initialisé avec get_default_temp() = '{self.tempname}'")

        # Temp Rosée setup
        self.defaultDewPointMarginC: float = 2.0
        self.dewPointMarginC: float = self.defaultDewPointMarginC
        self.asservTempRosee: bool = False  # Désactivé par défaut

        # Paramétrage initial de l'asservissement.
        #
        # Principe de dimensionnement de Kp :
        # On souhaite qu'un écart égal à `dewPointMarginC` (défaut +2°C) produise
        # une commande de 100 % (pleine puissance). Cela garantit que dès que
        # l'objet atteint le point de rosée — soit la limite de sécurité — le
        # chauffage est activé à pleine puissance.
        #   Kp = 100 / dewPointMarginC = 100 / 2 = 50
        #
        # Lecture pratique de Kp = 50 :
        # - `pid_output` est un pourcentage PWM entre 0 et 100.
        # - Un écart de +1.0°C produit 50 % de commande, soit ~18 W sur 36 W max.
        # - Exemples pour dewPointMarginC = 2°C :
        #   erreur = 0.5°C (objet à 1.5°C au-dessus rosée) ->  25 % PWM ->  ~9 W
        #   erreur = 1.0°C (objet à 1.0°C au-dessus rosée) ->  50 % PWM -> ~18 W
        #   erreur = 2.0°C (objet au niveau de la rosée)    -> 100 % PWM -> ~36 W
        #
        # Lien avec la marge sur le point de rosée (`dewPointMarginC`) :
        # En mode asservissement rosée, la consigne est construite comme :
        #   cmdTemp = tempRosee + dewPointMarginC
        # Dans l'état stable (objet à la consigne, erreur = 0) la commande tombe
        # à zéro : aucune chauffe nécessaire. À l'approche de la condensation,
        # l'erreur croît linéairement et la pleine puissance est atteinte
        # exactement quand l'objet touche le point de rosée — sans dépasser cette
        # limite avant d'avoir utilisé toute la capacité de chauffe disponible.
        #
        # `Ki` et `Kd` démarrent à zéro pour obtenir une mise en route douce :
        # - pas d'intégrale au démarrage, donc moins de risque de windup tant que
        #   l'inertie thermique du système réel n'est pas caractérisée ;
        # - pas de dérivée au démarrage, donc moins de sensibilité au bruit des
        #   sondes de température.
        #
        # Ce choix de départ est volontairement conservateur : la boucle commence
        # avec une réaction essentiellement proportionnelle, facile à comprendre,
        # puis l'utilisateur peut activer l'auto-ajustement une fois le
        # comportement thermique observé sur le montage réel.
        self.thread: Optional[threading.Thread] = None
        self.autoUpdateKpKiKd: bool = False  # Par défaut OFF au premier lancement
        self.Kp: float = 0.0
        self.Ki: float = 0.0
        self.Kd: float = 0.0
        self.setDefaultKpKiKd()

        self.cmdTemp: float = 10  # Consigne par défaut 10°C
        self.poids_objet: float = 1
        self.puissance_max: float = 12 * 3
        self.minTemp: float = MinTemp
        self.maxTemp: float = MaxTemp
        self.maxTempFailCount: int = 30
        self._running: bool = False
        load_result = self.load()
        print(f"[DEBUG AstraPwm.__init__] {self.name}: après load(), tempname='{self.tempname}', load_result={load_result}")

    def end(self) -> None:
        """Arrête l'asservissement et coupe la sortie chauffante."""
        self.stopAserv()
        self.set_ratio(0)
        self.pwmActor.close()

    def get_default_temp(self) -> Optional[str]:
        """Retourne le premier capteur 1-Wire disponible, sinon `None`."""
        tempNames = self.oneWireFetcher.get_listTemp()
        print(f"[DEBUG AstraPwm.get_default_temp] tempNames disponibles = {tempNames}")
        if len(tempNames) > 0:
            result = tempNames[0]
            print(f"[DEBUG AstraPwm.get_default_temp] retourne '{result}'")
            return result
        print(f"[DEBUG AstraPwm.get_default_temp] aucun capteur disponible, retourne None")
        return None

    ######## Accessors 
    def get_name(self) -> str:
        return self.name

    # Control temperature command
    def set_cmdTemp(self, set_cmdTemp: float) -> None:
        try:
            self.cmdTemp = int(set_cmdTemp)
        except:
            pass

    def get_cmdTemp(self) -> float:
        # Log pour debug : vérifier la valeur retournée
        print(f"[DEBUG get_cmdTemp] {self.name}: cmdTemp={self.cmdTemp}, asservTempRosee={self.asservTempRosee}")
        return self.cmdTemp

    def get_deltaTempRosee(self) -> float:
        return self.dewPointMarginC

    def set_asservTempRosee(self) -> None:
        self.asservTempRosee = True

    def unset_asservTempRosee(self) -> None:
        self.asservTempRosee = False

    def set_deltaTempRosee(self, deltaTempRosee: float) -> None:
        self.dewPointMarginC = deltaTempRosee

    def updateCmdTempfromTempRosee(self) -> None:
        """Met à jour la consigne à partir du point de rosée si ce mode est actif."""
        if self.asservTempRosee: 
            # Récupérer les valeurs brutes et filtrées pour le logging
            temp_brute = self.bmeFetcher.get_bmeTemp()
            temp_filtree = self.bmeFetcher.get_filteredTemp()
            hum_brute = self.bmeFetcher.get_bmeHumidity()
            hum_filtree = self.bmeFetcher.get_filteredHumidity()
            rosee_brute = self.get_bmeTempRosee()
            rosee_filtree = self.bmeFetcher.get_filteredDewPoint()
            
            # Utiliser le point de rosée filtré pour éviter les variations
            tempRosee = rosee_filtree
            
            # Si le filtre n'est pas encore initialisé, utiliser la valeur brute
            if tempRosee == self.ROSEEUNAVAIL or tempRosee == self.TEMPUNAVAIL:
                tempRosee = rosee_brute
            
            # Vérifier si le point de rosée est disponible
            if tempRosee != self.ROSEEUNAVAIL and tempRosee != self.TEMPUNAVAIL:
                cmdTemp = tempRosee + self.dewPointMarginC
                cmdTemp_avant_arrondi = cmdTemp
                # Arrondir la consigne à 0.1°C près pour éviter les variations d'affichage
                cmdTemp_avant = self.cmdTemp  # Sauvegarder l'ancienne valeur pour debug
                self.cmdTemp = round(cmdTemp * 10.0) / 10.0
                
                # Log détaillé des valeurs qui déterminent la consigne
                print(f"[CONSIGNE] {self.name}: "
                      f"Temp(brute={temp_brute:.3f}°C, filtrée={temp_filtree:.3f}°C) | "
                      f"Hum(brute={hum_brute:.2f}%, filtrée={hum_filtree:.2f}%) | "
                      f"Rosée(brute={rosee_brute:.3f}°C, filtrée={rosee_filtree:.3f}°C) | "
                      f"Delta={self.dewPointMarginC:.1f}°C | "
                      f"Consigne(avant_arrondi={cmdTemp_avant_arrondi:.3f}°C, après_arrondi={self.cmdTemp:.1f}°C, avant_update={cmdTemp_avant:.1f}°C)")
            else:
                # Si le point de rosée n'est pas disponible, garder la consigne actuelle
                print(f"[CONSIGNE] {self.name}: Point de rosée indisponible (tempRosee={tempRosee:.3f}°C), conservation de cmdTemp={self.cmdTemp:.1f}°C")

    # Asserv Parameters
    def get_autoUpdateKpKiKd(self) -> bool:
        return self.autoUpdateKpKiKd

    def setDefaultKpKiKd(self) -> None:
        """Réapplique les gains PID par défaut de l'application.

        Les valeurs par défaut sont:
        - Kp = 100 / dewPointMarginC (pleine puissance pour un défaut de marge)
        - Ki = 0
        - Kd = 0
        """
        if self.dewPointMarginC > 0:
            self.Kp = 100.0 / self.dewPointMarginC
        else:
            self.Kp = 0.0
        self.Ki = 0.0
        self.Kd = 0.0

    def set_autoUpdateKpKiKd(self) -> None:
        self.autoUpdateKpKiKd=True

    def unset_autoUpdateKpKiKd(self) -> None:
        self.autoUpdateKpKiKd=False

    def get_Kp(self) -> float:
        return self.Kp

    def set_kp(self, Kp: float) -> None:
        self.Kp=max(0, min(Kp, 100))

    def get_Ki(self) -> float:
        return self.Ki

    def set_Ki(self, Ki: float) -> None:
        self.Ki=max(0, min(Ki, 100))

    def get_Kd(self) -> float:
        return self.Kd

    def set_Kd(self, Kd: float) -> None:
        self.Kd=max(0, min(Kd, 100))

    # Associated sensor
    def get_listTemp(self) -> list[str]:
       return self.oneWireFetcher.get_listTemp()

    def get_temp(self) -> float:
        return self.oneWireFetcher.get_temp(self.tempname)

    def get_associateTemp(self) -> Optional[str]:
        # Si tempname est None, on essaie de récupérer un capteur par défaut
        if self.tempname is None:
            default_temp = self.get_default_temp()
            if default_temp is not None:
                print(f"[DEBUG AstraPwm.get_associateTemp] {self.name}: tempname était None, mise à jour avec '{default_temp}'")
                self.tempname = default_temp
        return self.tempname

    def _set_associateTemp(self, name: str) -> bool:
        retval = False
        iteration=4
        #while ((iteration > 0) and (not(retval))):
        #    if name in self.oneWireFetcher.get_listTemp():
        #        self.tempname = name
        #        retval= True
        #    else:
        #        time.sleep(0.1)
        self.tempname = name
        retval=True
        return retval

    def set_associateTemp(self, name: str) -> bool:
        if self._set_associateTemp(name):
            return True
        else:
            return False

    # Environmental sensor
    def get_bmeTemp(self) -> float:
        return self.bmeFetcher.get_bmeTemp()

    def get_bmePressure(self) -> float:
        return self.bmeFetcher.get_bmePressure()

    def get_bmeHumidity(self) -> float:
        return self.bmeFetcher.get_bmeHumidity()

    def get_bmeTempRosee(self) -> float:
        return self.bmeFetcher.get_bmeTempRosee()
    
    def has_humidity_sensor(self) -> bool:
        """Retourne True si le capteur a un capteur d'humidité (BME280), False sinon (BMP280)"""
        return self.bmeFetcher.has_humidity_sensor()
    
    def set_manual_humidity(self, humidity: float) -> None:
        """Définit manuellement l'humidité (pour BMP280 sans capteur d'humidité)"""
        self.bmeFetcher.set_manual_humidity(humidity)

    def print_status(self) -> None:
        try:
            TargetVoltage=self.ratio*12/100
            print(f"{self.name}:{self.ratio} TargetVoltage={TargetVoltage}")
        except:
            print("!!!!!!!!!!!!!!!")

    # set output
    def set_ratio(self, ratio: float) -> None:
        """Applique un rapport cyclique en pourcentage sur la sortie PWM."""
        old_ratio = self.ratio
        self.ratio=max(0, min(100,int(ratio*10)/10))
        duty=self.period_ms*self.ratio/100.0
        # #region agent log
        _write_debug_log("debug-session", "runtime", "A,B,D,E", "AstraPwm.py:set_ratio", "Setting PWM ratio", {"name":self.name,"gpio":self.inacaract["gpio"],"old_ratio":old_ratio,"new_ratio":self.ratio,"duty_ms":duty})
        # #endregion
        self.pwmActor.setStepPercent(self.ratio)
        # Log périodique pour debug (toutes les 10 changements significatifs)
        if not hasattr(self, '_set_ratio_counter'):
            self._set_ratio_counter = 0
        self._set_ratio_counter += 1
        if abs(old_ratio - self.ratio) > 1.0 or self._set_ratio_counter % 10 == 0:
            print(f"[DEBUG AstraPwm.set_ratio] {self.name}: ratio={self.ratio:.1f}% => duty={duty:.3f}ms (gpio {self.inacaract['gpio']})")

    def get_ratio(self) -> int:
        #print("AstraPwm.get_ratio(",self.ratio,")")
        return int(self.ratio)

    def _getAservLoopPeriodS(self) -> float:
        """Calcule la période de boucle d'asservissement selon la cadence PWM réelle."""
        minLoopPeriodS = 15.0
        pwmUpdatePeriodS = 0.0
        try:
            pwmUpdatePeriodS = float(self.pwmActor.getPwmUpdatePeriodS())
        except Exception:
            pwmUpdatePeriodS = 0.0

        # La boucle est bornée par un minimum de 15 s pour respecter
        # l'inertie thermique et par la cadence de mise à jour PWM effective.
        return max(minLoopPeriodS, pwmUpdatePeriodS)

    def _auto_tune_pid_lms(self) -> None:
        """Boucle d'asservissement thermique exécutée dans un thread dédié."""
        # Initialisation des coefficients PID
        # Période de régulation (commande PWM):
        # - elle suit la cadence réellement applicable par le driver PWM ;
        # - elle est bornée à 15 s min pour éviter de sur-piloter un système
        #   thermique qui répond en minutes.
        step_time: float = self._getAservLoopPeriodS()
        # Période minimale d'apprentissage (mise à jour des gains PID) :
        # on force une adaptation lente, au plus une fois toutes les 5 minutes,
        # pour laisser le temps à la physique (inertie + diffusion thermique)
        # de montrer l'effet réel de la commande précédente.
        minLearningPeriodS: float = 5 * 60.0
        # Taux d'apprentissage LMS (Least Mean Squares) utilisé pour l'auto-ajustement
        # des coefficients Kp, Ki, Kd quand `autoUpdateKpKiKd` est activé.
        #
        # Principe: deux échelles de temps distinctes.
        # 1) Régulation: calcul de la sortie PID à chaque `step_time`.
        # 2) Apprentissage: mise à jour de Kp/Ki/Kd beaucoup plus rare
        #    (fenêtre minimale de 5 min).
        #
        # Avec step_time >= 15 s, la régulation tourne au plus à 4 it/min.
        # Pour un système avec τ ≈ 3..10 min, cela représente 12..40 calculs PID
        # par constante de temps, ce qui est suffisant pour suivre l'évolution
        # thermique sans excès de bruit sur la commande.
        #
        # À chaque itération, le coefficient est décalé de :
        #   ΔKp = learning_rate × error
        # Contrainte de stabilité : ne pas dépasser ~1 % de la valeur initiale de Kp
        # (≈ 50) sur une constante de temps (N ≈ 5 itérations pour τ = 5 min) avec
        # une erreur typique de 1°C :
        #   learning_rate < 0.01 × Kp₀ / (|error| × N)
        #                 = 0.01 × 50  / (1 × 5)
        #                 = 0.1
        # Le learning rate est adapté à `step_time` pour conserver un effet
        # d'apprentissage comparable par unité de temps: référence 1e-3 à 15 s.
        # Exemple: si step_time double, le pas par mise à jour double aussi,
        # afin de ne pas "ralentir" artificiellement l'apprentissage horaire.
        #
        # En complément, la fenêtre minimale de 5 min impose que les gains
        # ne changent qu'après accumulation d'assez d'information thermique.
        # Cela évite le "chasing" des fluctuations courtes et améliore la
        # robustesse sur des charges à forte inertie.
        referenceStepS: float = 15.0
        learning_rate: float = 1e-3 * (step_time / referenceStepS)
        learningWindowIterationCount: int = max(1, int(round(minLearningPeriodS / step_time)))
        learningWindowCounter: int = 0
        lastpid_output=0

        # Mettre à jour la consigne depuis le point de rosée AVANT de calculer l'erreur initiale
        self.updateCmdTempfromTempRosee()
        
        error = self.get_cmdTemp() - self.get_temp()
        integralNbVal = 10
        # Initialiser l'intégrale à 0 pour éviter un pic au démarrage
        # Elle se remplira progressivement avec les vraies valeurs d'erreur
        integral = 0.0
        integralList = [0.0] * integralNbVal
        prev_error = error  # Initialiser prev_error avec l'erreur actuelle pour éviter un pic dérivé

        while self._running:
            if self.tempname is not None:
                failCount = self.oneWireFetcher.get_failCount(self.tempname)
                if failCount > self.maxTempFailCount:
                    print(
                        f"[SECURITY] {self.name}: arrêt asservissement, "
                        f"trop d'échecs capteur '{self.tempname}' "
                        f"(failCount={failCount} > {self.maxTempFailCount})"
                    )
                    self._running = False
                    self.set_ratio(0)
                    break

            self.updateCmdTempfromTempRosee()
            error = self.get_cmdTemp() - self.get_temp()
            integralList.append(error)
            # Calcul de l'intégrale glissante
            integral = integral - integralList.pop(0) + error

            # Calcul de la sortie du PID avec les coefficients PID actuels
            pid_output = self.Kp * error + self.Ki * integral + self.Kd * (error - prev_error)

            # Gestion de la saturation de pid_output entre 0 et 100
            pid_output = max(0, min(pid_output, 100))

            # Mise à jour des coefficients PID si la sortie n'est pas saturée.
            # L'apprentissage n'est autorisé qu'une fois par fenêtre minimale
            # (5 minutes par défaut) pour respecter l'inertie thermique.
            learningWindowCounter += 1
            shouldUpdateLearning = learningWindowCounter >= learningWindowIterationCount
            if self.autoUpdateKpKiKd and shouldUpdateLearning and pid_output < 100 and pid_output > -100:
                self.Kp -= learning_rate * error
                self.Ki += learning_rate * integral
                self.Kd -= learning_rate * (error - prev_error)
                # Gestion de la saturation des coefficients PID entre 0 et 100
                self.Kp = max(0, min(self.Kp, 100))
                self.Ki = max(0, min(self.Ki, 100))
                self.Kd = max(0, min(self.Kd, 100))
                learningWindowCounter = 0

            pid_output = max(0, min(pid_output, 100))
            # Log PID pour debug
            print(f"[PID] {self.name}: cmd={self.get_cmdTemp():.1f}°C, temp={self.get_temp():.1f}°C, error={error:.2f}°C, pid={pid_output:.1f}%, Kp={self.Kp:.3f}, Ki={self.Ki:.3f}, Kd={self.Kd:.3f}, integral={integral:.1f}")
            self.set_ratio(pid_output)
            time.sleep(step_time)
        self.set_ratio(0)

    def startAserv(self) -> None:
        """Démarre le thread d'asservissement s'il n'est pas déjà actif."""
        if not self._running:
            # test If I did launch a thread previously and wait for it to end
            if self.thread != None:
                self.thread.join()    
            self._running = True
            self.thread = threading.Thread(target=self._auto_tune_pid_lms)
            self.thread.start()

    def stopAserv(self) -> None:
        """Demande l'arrêt de la boucle d'asservissement."""
        if self._running:
            self._running = False
            

    def isAserv(self) -> bool:
        return self._running


    def load(self) -> bool:
        """Charge la configuration persistée et tente une autoconfiguration initiale."""
        variables_dict: dict[str, Any] = {}
        filename = "sauve"+self.name+".json"
        chemin_complet=Path.home() / ".AstrAlim"  / filename
        print(f"[DEBUG AstraPwm.load] {self.name}: fichier de sauvegarde = {chemin_complet}, existe = {chemin_complet.exists()}")
        if chemin_complet.exists():
            with open(chemin_complet, "r") as f:
                variables_dict = json.load(f)
            print(f"[DEBUG AstraPwm.load] {self.name}: contenu du fichier = {variables_dict}")
            if "Kp" in variables_dict and "Ki"  in variables_dict  and "Kd"  in variables_dict:
                self.Kp = variables_dict["Kp"]
                self.Ki = variables_dict["Ki"]
                self.Kd = variables_dict["Kd"]
            # Charger l'état de l'auto-calcul PID (par défaut False si absent)
            if "autoUpdateKpKiKd" in variables_dict:
                self.autoUpdateKpKiKd = variables_dict["autoUpdateKpKiKd"]
            else:
                # Si absent, utiliser la valeur par défaut (False)
                self.autoUpdateKpKiKd = False
            if "tempname" in variables_dict:
                saved_tempname = variables_dict["tempname"]
                print(f"[DEBUG AstraPwm.load] {self.name}: tempname trouvé dans sauvegarde = '{saved_tempname}', tempname actuel = '{self.tempname}'")
                result = self._set_associateTemp(saved_tempname)
                print(f"[DEBUG AstraPwm.load] {self.name}: après _set_associateTemp(), tempname = '{self.tempname}', result = {result}")
                return result
            else:
                print(f"[DEBUG AstraPwm.load] {self.name}: pas de tempname dans sauvegarde, tempname reste = '{self.tempname}'")
                return False
        else:
            print(f"[DEBUG AstraPwm.load] {self.name}: fichier de sauvegarde n'existe pas")
            # Autoconfiguration : si aucun fichier de config n'existe pour les deux instances,
            # on assigne automatiquement les capteurs disponibles
            chemin_pwm1 = Path.home() / ".AstrAlim" / "sauveAstraPwm1.json"
            chemin_pwm2 = Path.home() / ".AstrAlim" / "sauveAstraPwm2.json"
            
            # Vérifier si c'est une première installation (aucun des deux fichiers n'existe)
            if not chemin_pwm1.exists() and not chemin_pwm2.exists():
                print(f"[DEBUG AstraPwm.load] {self.name}: Première installation détectée - autoconfiguration des capteurs")
                available_sensors = self.get_listTemp()
                print(f"[DEBUG AstraPwm.load] {self.name}: Capteurs disponibles = {available_sensors}")
                
                if len(available_sensors) >= 2:
                    # Assigner automatiquement : premier capteur à AstraPwm1, second à AstraPwm2
                    if self.name == "AstraPwm1":
                        assigned_sensor = available_sensors[0]
                        print(f"[DEBUG AstraPwm.load] {self.name}: Autoconfiguration avec '{assigned_sensor}' (premier capteur)")
                        self.tempname = assigned_sensor
                        self.save()  # Sauvegarder immédiatement
                        return True
                    elif self.name == "AstraPwm2":
                        assigned_sensor = available_sensors[1]
                        print(f"[DEBUG AstraPwm.load] {self.name}: Autoconfiguration avec '{assigned_sensor}' (second capteur)")
                        self.tempname = assigned_sensor
                        self.save()  # Sauvegarder immédiatement
                        return True
                elif len(available_sensors) == 1:
                    # Un seul capteur : assigner au premier (AstraPwm1)
                    if self.name == "AstraPwm1":
                        assigned_sensor = available_sensors[0]
                        print(f"[DEBUG AstraPwm.load] {self.name}: Autoconfiguration avec '{assigned_sensor}' (un seul capteur disponible)")
                        self.tempname = assigned_sensor
                        self.save()
                        return True
                    else:
                        print(f"[DEBUG AstraPwm.load] {self.name}: Un seul capteur disponible, non assigné à AstraPwm2")
                else:
                    print(f"[DEBUG AstraPwm.load] {self.name}: Aucun capteur disponible pour l'autoconfiguration")
            
            # Si tempname est None et qu'il n'y a pas de fichier de sauvegarde, 
            # on essaie de récupérer un capteur par défaut maintenant
            if self.tempname is None:
                default_temp = self.get_default_temp()
                if default_temp is not None:
                    print(f"[DEBUG AstraPwm.load] {self.name}: tempname était None, mise à jour avec get_default_temp() = '{default_temp}'")
                    self.tempname = default_temp
                else:
                    print(f"[DEBUG AstraPwm.load] {self.name}: tempname est None et aucun capteur disponible")
            return False

    def save(self) -> None:
        """Sauvegarde la configuration utilisateur dans le répertoire personnel."""
        variables_dict: dict[str, Any] = {
                "name":self.name, 
                "tempname":self.tempname,
                "Kp":self.Kp,
                "Ki":self.Ki,
                "Kd":self.Kd,
                "autoUpdateKpKiKd":self.autoUpdateKpKiKd,
                }
        chemin_complet=Path.home() / ".AstrAlim"
        chemin_complet.mkdir(parents=True, exist_ok=True)
        filename = "sauve"+self.name+".json"
        chemin_complet = chemin_complet / filename
        with open(chemin_complet, "w") as f:
            json.dump(variables_dict, f, indent=4)

if __name__ == '__main__':
    import signal
    import sys

    
    print(f"Running on : {RaspberryPiModel().getPi()}")
  
    astrapwm1=AstraPwm("AstraPwm1")
    astrapwm2=AstraPwm("AstraPwm2")
    
    def signal_handler(sig, frame):
        print('You pressed Ctrl+C!')
        astrapwm1.end()
        astrapwm2.end()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    modulo=50
    duty=0
    while True:
        astrapwm1.set_ratio(duty%modulo)
        astrapwm1.print_status()
        astrapwm2.set_ratio((100-duty)%modulo)
        astrapwm2.print_status()
        for name in astrapwm2.get_listTemp():
            astrapwm2.set_associateTemp(name)
            print(name,"=", astrapwm2.get_temp())
        print("bmeTemp=", astrapwm1.get_bmeTemp(), "bmeHum=", astrapwm1.get_bmeHumidity(), "Rosee=", astrapwm1.get_bmeTempRosee())
        duty=(duty+1)%101
        time.sleep(1)
    
