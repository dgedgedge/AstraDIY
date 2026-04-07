#!/usr/bin/env python3
# GPIO used PA17
import logging
from lib.ina219 import INA219
import time
from lib.astra_com_fetcher import AstraComFetcher
from lib.astra_com_device import AstraComDevice


class AstraIna(AstraComDevice):
    RANGE_16V = INA219.RANGE_16V  # Range 0-16 volts
    RANGE_32V = INA219.RANGE_32V  # Range 0-32 volts

    GAIN_1_40MV = INA219.GAIN_1_40MV  # Maximum shunt voltage 40mV
    GAIN_2_80MV = INA219.GAIN_2_80MV  # Maximum shunt voltage 80mV
    GAIN_4_160MV = INA219.GAIN_4_160MV  # Maximum shunt voltage 160mV
    GAIN_8_320MV = INA219.GAIN_8_320MV  # Maximum shunt voltage 320mV
    GAIN_AUTO = INA219.GAIN_AUTO  # Determine gain automatically

    ADC_9BIT = INA219.ADC_9BIT  # 9-bit conversion time  84us.
    ADC_10BIT = INA219.ADC_10BIT  # 10-bit conversion time 148us.
    ADC_11BIT = INA219.ADC_11BIT  # 11-bit conversion time 2766us.
    ADC_12BIT = INA219.ADC_12BIT  # 12-bit conversion time 532us.
    ADC_2SAMP = INA219.ADC_2SAMP  # 2 samples at 12-bit, conversion time 1.06ms.
    ADC_4SAMP = INA219.ADC_4SAMP  # 4 samples at 12-bit, conversion time 2.13ms.
    ADC_8SAMP = INA219.ADC_8SAMP  # 8 samples at 12-bit, conversion time 4.26ms.
    ADC_16SAMP = INA219.ADC_16SAMP  # 16 samples at 12-bit,conversion time 8.51ms
    ADC_32SAMP = INA219.ADC_32SAMP  # 32 samples at 12-bit, conversion time 17.02ms.
    ADC_64SAMP = INA219.ADC_64SAMP  # 64 samples at 12-bit, conversion time 34.05ms.
    ADC_128SAMP = INA219.ADC_128SAMP  # 128 samples at 12-bit, conversion time 68.10ms.

    # Dictionnaire associant les noms aux informations sur les capteurs INA219
    ina219_set = {
            "AstraDc1": {"ispwm":False, "busnum":1, "address": 0x41, "shunt_ohms": 0.01, "max_expected_amps": 6, "pin": 37,
                         "force_abs_current_power": True,
                         "bus_adc":INA219.ADC_12BIT, "shunt_adc":INA219.ADC_12BIT },
            "AstraDc2": {"ispwm":False, "busnum":1, "address": 0x44, "shunt_ohms": 0.01, "max_expected_amps": 6, "pin": 38,
                         "force_abs_current_power": True,
                         "bus_adc":INA219.ADC_12BIT, "shunt_adc":INA219.ADC_12BIT },
            "AstraDc3": {"ispwm":False, "busnum":1, "address": 0x46, "shunt_ohms": 0.01, "max_expected_amps": 6, "pin": 40,
                         "force_abs_current_power": True,
                         "bus_adc":INA219.ADC_12BIT, "shunt_adc":INA219.ADC_12BIT },
            "AstraPwm1": {"ispwm":True, "busnum":1, "address": 0x49, "shunt_ohms": 0.01, "max_expected_amps": 6, "chip":None, "pwm":1, 
                         "bus_adc":INA219.ADC_12BIT, "shunt_adc":INA219.ADC_12BIT },
            "AstraPwm2": {"ispwm":True, "busnum":1, "address": 0x4d, "shunt_ohms": 0.01, "max_expected_amps": 6, "chip":None, "pwm":2, 
                         "bus_adc":INA219.ADC_12BIT, "shunt_adc":INA219.ADC_12BIT },
            "AstOnStep": {"ispwm":False, "busnum":1, "address": 0x40, "shunt_ohms": 0.005, "max_expected_amps": 6, "chip":None, "pwm":2, 
                         "bus_adc":INA219.ADC_12BIT, "shunt_adc":INA219.ADC_12BIT }
    }
    MIN_VALID_SAMPLES_ABS = 3
    MIN_VALID_SAMPLES_RATIO = 0.40
    PUBLISH_SMOOTH_ALPHA = 0.35

    @classmethod
    def getListNames(cls):
        return AstraIna.ina219_set.keys()

    @classmethod
    def exitAll(cls):
        AstraComFetcher.exitAll()

    def __init__(self, shunt_ohms=-1, max_expected_amps=-1, busnum=-1, address=-1, name="", log_level=logging.ERROR):
        eachStep = False
        if name != "" and name in self.ina219_set:
            eachStep = bool(self.ina219_set[name].get("ispwm", False))

        self.eachStep: bool = eachStep
        self.ispwm: bool = eachStep
        self.configured:bool=False
        self.configurationSend:bool=False

        self.name:str=name
        # Caracteristiques
        self.address=address
        self.voltage_range=-1
        self.gain=-1
        self.bus_adc=-1
        self.shunt_adc=-1
                 
        # Last collection
        self.pingOk=True
        self._intPeriodS:float=0.0

        self._voltageV:float=0.0
        self._shuntVoltagemV:float=0.0
        self._currentmA:float=0.0
        self._powermW:float=0.0
        self._energiemWS:float=0.0

        self._publishedVoltageV: float = 0.0
        self._publishedShuntVoltagemV: float = 0.0
        self._publishedCurrentmA: float = 0.0
        self._publishedPowermW: float = 0.0

        self._cycleVoltageSumV: float = 0.0
        self._cycleShuntVoltageSummV: float = 0.0
        self._cycleCurrentSummA: float = 0.0
        self._cyclePowerSummW: float = 0.0
        self._cycleSampleCount: int = 0
    
        self.ina219:INA219= None

        if self.name == "":
            if shunt_ohms==-1 or max_expected_amps==-1 or busnum==-1 or address==-1:
                raise Exception("If name notspecified call AstraIna(shunt_ohms, max_expected_amps, busnum, address")
            # Temp fetcher
            self.ina219 = INA219(shunt_ohms=shunt_ohms, max_expected_amps=max_expected_amps, busnum=busnum, address=address, log_level=log_level)
        else:
            if self.name in self.ina219_set:
                self.caract=self.ina219_set[self.name]  # Utiliser self.name au lieu de name pour cohérence
                # Définir self.address AVANT de créer l'INA219 et d'appeler configure()
                self.address = self.caract["address"]
                print(f"[DEBUG AstraIna.__init__] {self.name}: Configuration trouvée, address=0x{self.address:x}, pwm={self.caract.get('pwm', 'N/A')}")
                self.ina219 = INA219(
                    shunt_ohms=self.caract["shunt_ohms"], 
                    max_expected_amps=self.caract["max_expected_amps"], 
                    busnum=self.caract["busnum"], 
                    address=self.address, 
                    log_level=log_level)
                self.configure(bus_adc=self.caract["bus_adc"], shunt_adc=self.caract["shunt_adc"])
                print(f"[DEBUG AstraIna.__init__] {self.name}: INA219 créé et configuré, address=0x{self.address:x}")
            else:
                raise Exception("Unkown AstraIna")
        super().__init__(name=name, eachStep=eachStep)

    def startMeasurement(self, step: int, integrationDurationS: float) -> None:
        """
        As the INA may loose it's configuration, it is necessary to:
        1- Check if the ina is present.
        2- Send the configuration if it is present.
        Collect of the data shall be done through getMeasurement
        The method is not threadsafe and shall be called by a uniq thread.
        """
        pingOk=False
        self.configurationSend=False
        try:
            if self.ina219.ping():
                pingOk=True
            if pingOk:
                try:
                    self.ina219.configure(
                        voltage_range=self.voltage_range, 
                        gain=self.gain, 
                        bus_adc=self.bus_adc, 
                        shunt_adc=self.shunt_adc)
                    self.configurationSend = True
                    self.pingOk = True
                except (OSError, IOError) as e:
                    # Erreur I2C - le capteur peut être temporairement indisponible
                    self.pingOk = False
                    self.configurationSend = False
                    # Logger les erreurs pour les PWM
                    if "Pwm" in self.name:
                        if not hasattr(self, '_ping_error_counter'):
                            self._ping_error_counter = 0
                        self._ping_error_counter += 1
                        if self._ping_error_counter % 10 == 0:  # Logger toutes les 10 erreurs
                            print(f"[DEBUG startMeasurement] {self.name}: ERREUR I2C configure - {e}")
        except (OSError, IOError) as e:
            # Erreur I2C lors du ping
            self.pingOk = False
            self.configurationSend = False
            if "Pwm" in self.name:
                if not hasattr(self, '_ping_error_counter'):
                    self._ping_error_counter = 0
                self._ping_error_counter += 1
                if self._ping_error_counter % 10 == 0:  # Logger toutes les 10 erreurs
                        print(f"[DEBUG startMeasurement] {self.name}: ERREUR I2C ping - {e}")
            pass            
        
    def getPingOK(self)->bool:
        """
        Return the last ping status.
        """
        return self.pingOk
    
    def getMeasurement(self, step: int, integrationDurationS: float) -> None:
        """
        Do the INA iteraction.
        The user shall have called startMeasurement each time before calling this method.
        Collects measures of the INA for publication.
        The method is not threadsafe and shall be called by a uniq thread.
        It is considered that the last measure OK is cummulated in the energy.
        """
        if not self.configurationSend:
            # Logger si configurationSend est False pour les PWM
            if "Pwm" in self.name:
                if not hasattr(self, '_config_false_counter'):
                    self._config_false_counter = 0
                self._config_false_counter += 1
                if self._config_false_counter % 25 == 0:  # Logger toutes les 25 fois (~10 secondes)
                    print(f"[DEBUG getData] {self.name}: configurationSend=False, pingOK={self.pingOk} - lecture ignorée")
            return
        
        try:
            deltatimeS = integrationDurationS
            if not self.ina219.current_overflow():
                # Lire les valeurs brutes AVANT le max(..., 0.0) pour voir les valeurs négatives potentielles
                raw_shunt_mV = self.ina219.shunt_voltage()
                raw_voltage_V = float(self.ina219.voltage())
                raw_current_mA = float(self.ina219.current())
                raw_power_mW = float(self.ina219.power())
                
                # L'inversion de polarité n'est plus nécessaire pour AstraPwm1
                # Les logs montrent que le courant brut est déjà positif (469.939mA)
                # L'inversion le rendait négatif, puis max(..., 0.0) le mettait à 0
                
                force_abs = False
                if hasattr(self, "caract"):
                    force_abs = bool(self.caract.get("force_abs_current_power", False))

                # Keep sign for diagnostic by default.
                # Some boards can wire shunt polarity opposite to "consumption"
                # direction: in this case use absolute values for displayed/published
                # current and power to avoid masking real load as zero.
                if force_abs:
                    self._currentmA = abs(raw_current_mA)
                    self._powermW = abs(raw_power_mW)
                else:
                    self._currentmA = raw_current_mA
                    self._powermW = raw_power_mW

                self._shuntVoltagemV = raw_shunt_mV
                self._voltageV = max(raw_voltage_V, 0.0)
                self._accumulateCycleAndPublish(step)
                
                # Debug détaillé pour les INA (toutes les 25 lectures environ)
                if not hasattr(self, '_debug_counter'):
                    self._debug_counter = 0
                self._debug_counter += 1
                if self._debug_counter % 25 == 0:
                    print(f"[DEBUG getData] {self.name}: RAW (après inversion si Pwm1) - shunt={raw_shunt_mV:.3f}mV, V={raw_voltage_V:.3f}V, I={raw_current_mA:.3f}mA, P={raw_power_mW:.3f}mW")
                    print(f"[DEBUG getData] {self.name}: AFTER max(0) - shunt={self._shuntVoltagemV:.3f}mV, V={self._voltageV:.3f}V, I={self._currentmA:.3f}mA, P={self._powermW:.3f}mW, overflow={self.ina219.current_overflow()}")
            else:
                # Logger si overflow
                if "Pwm" in self.name:
                    if not hasattr(self, '_overflow_counter'):
                        self._overflow_counter = 0
                    self._overflow_counter += 1
                    if self._overflow_counter % 10 == 0:
                        print(f"[DEBUG getData] {self.name}: CURRENT OVERFLOW détecté")
            energiemWS=self._powermW * deltatimeS
            self._energiemWS += energiemWS
            self._intPeriodS += deltatimeS
            self.pingOk = True
        except (OSError, IOError) as e:
            # Erreur I2C - le capteur peut être temporairement indisponible
            self.pingOk = False
            # Conserver les dernières valeurs valides
            if "Pwm" in self.name:
                if not hasattr(self, '_read_error_counter'):
                    self._read_error_counter = 0
                self._read_error_counter += 1
                if self._read_error_counter % 10 == 0:  # Logger toutes les 10 erreurs
                    print(f"[DEBUG getData] {self.name}: ERREUR I2C lecture - {e}")
            pass

    def _resetCycleAccumulators(self) -> None:
        """Reset per-cycle accumulators."""
        self._cycleVoltageSumV = 0.0
        self._cycleShuntVoltageSummV = 0.0
        self._cycleCurrentSummA = 0.0
        self._cyclePowerSummW = 0.0
        self._cycleSampleCount = 0

    def _publishCurrentCycleAverage(self) -> None:
        """Publish averages computed from the current cycle accumulators."""
        if self._cycleSampleCount <= 0:
            return

        if self.eachStep:
            cycle_steps = self.cycleStepCount if isinstance(self.cycleStepCount, int) and self.cycleStepCount > 0 else 10
            min_samples = max(self.MIN_VALID_SAMPLES_ABS, int(round(cycle_steps * self.MIN_VALID_SAMPLES_RATIO)))
            if self._cycleSampleCount < min_samples:
                return

        avg_voltage_v = self._cycleVoltageSumV / self._cycleSampleCount
        avg_shunt_mv = self._cycleShuntVoltageSummV / self._cycleSampleCount
        avg_current_ma = self._cycleCurrentSummA / self._cycleSampleCount
        avg_power_mw = self._cyclePowerSummW / self._cycleSampleCount

        if self.eachStep and (abs(self._publishedVoltageV) > 0.0 or abs(self._publishedCurrentmA) > 0.0):
            alpha = self.PUBLISH_SMOOTH_ALPHA
            self._publishedVoltageV = (alpha * avg_voltage_v) + ((1.0 - alpha) * self._publishedVoltageV)
            self._publishedShuntVoltagemV = (alpha * avg_shunt_mv) + ((1.0 - alpha) * self._publishedShuntVoltagemV)
            self._publishedCurrentmA = (alpha * avg_current_ma) + ((1.0 - alpha) * self._publishedCurrentmA)
            self._publishedPowermW = (alpha * avg_power_mw) + ((1.0 - alpha) * self._publishedPowermW)
        else:
            self._publishedVoltageV = avg_voltage_v
            self._publishedShuntVoltagemV = avg_shunt_mv
            self._publishedCurrentmA = avg_current_ma
            self._publishedPowermW = avg_power_mw

    def _accumulateCycleAndPublish(self, step: int) -> None:
        """Accumulate on each-step devices, publish immediately on non-each-step devices."""
        if not self.eachStep:
            self._publishedVoltageV = self._voltageV
            self._publishedShuntVoltagemV = self._shuntVoltagemV
            self._publishedCurrentmA = self._currentmA
            self._publishedPowermW = self._powermW
            return

        if step == 0 and self._cycleSampleCount > 0:
            self._publishCurrentCycleAverage()
            self._resetCycleAccumulators()

        self._cycleVoltageSumV += self._voltageV
        self._cycleShuntVoltageSummV += self._shuntVoltagemV
        self._cycleCurrentSummA += self._currentmA
        self._cyclePowerSummW += self._powermW
        self._cycleSampleCount += 1

    def onCycleConfigurationChanged(self, periodS: float, stepCount: int) -> None:
        """Update local cycle config and reset cycle averaging state."""
        super().onCycleConfigurationChanged(periodS, stepCount)
        self._resetCycleAccumulators()
    
    def configure(self, voltage_range=INA219.RANGE_16V, gain=INA219.GAIN_AUTO, bus_adc=INA219.ADC_12BIT, shunt_adc=INA219.ADC_12BIT):
        if self.configured:
            raise Exception("AstraIna already Configured")
        else:
            self.voltage_range=voltage_range
            self.gain=gain
            self.bus_adc=bus_adc
            self.shunt_adc=shunt_adc
            self.configured=True
            print(f"[DEBUG AstraIna.configure] {self.name}: Configuré")

    def __str__(self) -> str:
        displayName = self.name if self.name != "" else self.__class__.__name__
        addressText = f"0x{self.address:02x}" if self.address >= 0 else "N/A"
        return f"{displayName} address={addressText} ispwm={self.ispwm}"

    def __format__(self, formatSpec: str) -> str:
        return format(str(self), formatSpec)

    
    def getName(self)->str:
        return self.name

    def setName(self, name):
        self.name = name

    def voltageV(self)->float:
        """
        Return the last seen bus voltage in volts.
        """
        return self._publishedVoltageV

    def shuntVoltagemV(self)->float:
        """
        Return the last seen shunt voltage in millivolts.
        """
        return self._publishedShuntVoltagemV

    def shuntVoltageV(self)->float:
        """
        Return the last seen shunt voltage in millivolts.
        """
        return self.shuntVoltagemV() / 1000.0
    
    def currentmA(self)->float:
        """
        Return the bus current in milliamps.
        """
        return self._publishedCurrentmA

    def currentA(self)->float:
        """
        Return the bus current in Amps.
        """
        return (self.currentmA() / 1000.0)
    
    def powermW(self)->float:
        """
        Return the bus power consumption in milliwatts.
        """
        return self._publishedPowermW

    def powerW(self)->float:
        """
        Return the bus power consumption in Watts.
        """
        return (self.powermW() / 1000.0)
    
    def energiemWS(self)->float:
        """ 
        Return Cumulated Energie cosumption in mVAs mWs mJoules
        """
        return self._energiemWS
    
    def energieWS(self)->float:
        """ 
        Return Cumulated Energie cosumption in VAs Ws Joules
        """
        return (self._energiemWS / 1000.0)
        
    def intPeriodS(self)->float:
        return self._intPeriodS

    def getInfo(self) -> dict:
        """
        Retourne un snapshot des dernières valeurs publiées (sans accès I2C).

        Returns:
            dict avec name, address, pingOk, voltageV, shuntVoltagemV,
            currentmA, powermW, energiemWS, intPeriodS.
        """
        return {
            "name": self.name,
            "address": self.address,
            "pingOk": self.pingOk,
            "voltageV": self._publishedVoltageV,
            "shuntVoltagemV": self._publishedShuntVoltagemV,
            "currentmA": self._publishedCurrentmA,
            "powermW": self._publishedPowermW,
            "energiemWS": self._energiemWS,
            "intPeriodS": self._intPeriodS,
        }

    def getDiagnosticInfo(self)->dict:
        """
        Retourne des informations de diagnostic pour déboguer les mesures.
        Utile pour identifier les problèmes de mesure de courant.
        """
        try:
            if self.ina219 is None:
                return {"error": "INA219 not initialized"}
            
            # Vérifier si le capteur répond
            ping_ok = self.ina219.ping()
            
            if not ping_ok:
                return {"error": "INA219 not responding", "ping": False}
            
            # Lire les valeurs brutes
            try:
                shunt_voltage_mv = self.ina219.shunt_voltage()  # en millivolts
                bus_voltage_v = self.ina219.voltage()  # en volts
                current_ma = self.ina219.current()  # en milliamps
                power_mw = self.ina219.power()  # en milliwatts
                current_overflow = self.ina219.current_overflow()
                
                # Calculer le courant théorique à partir de la tension shunt
                # I = V_shunt / R_shunt
                if hasattr(self, 'caract'):
                    shunt_ohms = self.caract.get("shunt_ohms", 0.01)
                else:
                    shunt_ohms = 0.01  # valeur par défaut
                
                current_theoretical_ma = (shunt_voltage_mv / 1000.0) / shunt_ohms * 1000.0  # en mA
                
                return {
                    "ping": ping_ok,
                    "shunt_voltage_mv": shunt_voltage_mv,
                    "bus_voltage_v": bus_voltage_v,
                    "current_ma": current_ma,
                    "current_theoretical_ma": current_theoretical_ma,
                    "power_mw": power_mw,
                    "current_overflow": current_overflow,
                    "shunt_ohms": shunt_ohms,
                    "voltage_range": self.voltage_range if hasattr(self, 'voltage_range') else None,
                    "gain": self.gain if hasattr(self, 'gain') else None,
                    "configured": self.configured,
                    "configuration_sent": self.configurationSend
                }
            except Exception as e:
                return {"error": str(e), "ping": ping_ok}
        except Exception as e:
            return {"error": str(e)}    
            
    
if __name__ == "__main__":
    import signal
    import sys

    def signal_handler(sig, frame):
        print('You pressed Ctrl+C!')
        AstraIna.exitAll()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    
    newSyntax=True
    if newSyntax:
        print(AstraIna.getListNames())
        listIna=[]
        for name in AstraIna.getListNames():
            listIna.append(AstraIna(name=name))
        while True:
            time.sleep(1)
            ina219:AstraIna=listIna[0]
            # Conversion correcte: mWs -> mWh -> mAh
            # mWs / 3600 = mWh, puis mWh / tension_V = mAh
            # Pour l'exemple, on utilise 12V comme tension de référence
            tension_ref_V = 12.0
            total_mWh = sum(ina.energiemWS() for ina in listIna) / 3600.0
            total_mAh = total_mWh / tension_ref_V
            print(f"Energie={total_mAh:.3f} mAh (sous {tension_ref_V}V, {total_mWh:.3f} mWh)")
            print("===============================================================")
            for ina219 in listIna:
                name=ina219.getName()
                shunt_voltage = ina219.shuntVoltageV()
                bus_voltage = ina219.voltageV()
                current = ina219.currentA()
                power = ina219.powermW()
                energie = ina219.energiemWS() / 60 / 60
                intPeriod=ina219.intPeriodS()

                print(f"{name}: Shunt {shunt_voltage:+.3f}V, Bus {bus_voltage:+.3f}V Current: {current:+.3f}A, Power: {power:.3f}mW Energie: {energie:.3f}mWh  intPeriod: {intPeriod:.3f}s")
    else:
        # Dictionnaire associant les noms aux informations sur les capteurs INA219
        ina219_set = {
            "alim_1_i2c_41   ": {"address": 0x41, "shunt_ohms": 0.02, "max_expected_amps": 6},
            "alim_2_i2c_44   ": {"address": 0x44, "shunt_ohms": 0.02, "max_expected_amps": 6},
            "alim_3_i2c_45_5V": {"address": 0x46, "shunt_ohms": 0.02, "max_expected_amps": 6},
            "alim_4_i2c_47   ": {"address": 0x49, "shunt_ohms": 0.02, "max_expected_amps": 6},
            "alim_5_i2c_47   ": {"address": 0x4d, "shunt_ohms": 0.02, "max_expected_amps": 6}
        }


        # Créer une instance INA219Reader pour chaque capteur
        # Mettre à jour le dictionnaire ina219_addresses avec l'objet INA219Reader
        for name, info in ina219_set.items():
            address = info["address"]
            shunt_ohms = info["shunt_ohms"]
            max_expected_amps = info["max_expected_amps"]
            ina219_set[name]["ina219_object"] = AstraIna(address=address, shunt_ohms=shunt_ohms, max_expected_amps=max_expected_amps, busnum=1)
            ina219_set[name]["ina219_object"].configure(bus_adc=INA219.ADC_64SAMP, shunt_adc=INA219.ADC_64SAMP)

        while True:
            time.sleep(1)
            print("===============================================================")
            for name, info in ina219_set.items():
                ina219 = info["ina219_object"]
                shunt_voltage = ina219.shunt_voltage()
                bus_voltage = ina219.voltage()
                current = ina219.current()  # Retourne des milliamps
                power = ina219.power()
                # Note: energie() et intPeriod() n'existent pas dans INA219, seulement dans AstraIna
                # Conversion correcte: current est en milliamps, donc pour afficher en A, diviser par 1000
                currentA = current / 1000.0

                print(f"{name}: Shunt {shunt_voltage:+.3f}mV, Bus {bus_voltage:+.3f}V Current: {currentA:+.3f}A, Power: {power:.3f}mW")
