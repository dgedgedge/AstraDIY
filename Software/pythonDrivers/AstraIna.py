#!/bin/env python3
# GPIO used PA17
import logging
from ina219 import INA219
import threading
import time


class AstraInaFetcher(threading.Thread):
    _AstraInaFetcher=None

    def __init__(self):
        super().__init__()
        self.running = True
        self.listInalock = threading.Lock()
        self.listIna:list = []
        self.totalEnergiemWS:float=0

    @classmethod
    def get_instance(cls):
        if cls._AstraInaFetcher is None:
            cls._AstraInaFetcher = AstraInaFetcher()
            cls._AstraInaFetcher.start()
        return cls._AstraInaFetcher

    @classmethod
    def exitAll(cls):
        if not(cls._AstraInaFetcher is None):
            cls._AstraInaFetcher.stop()


    def run(self):
        while self.running:
            ina:AstraIna=None
            time.sleep(0.4)
            with self.listInalock:
                totalEnergiemWS:float=0.0
                for ina in self.listIna:
                    ina.sendConfiguration()
                
                time.sleep(0.1)

                for ina in self.listIna:
                    ina.getDataFromIna()
                    totalEnergiemWS+=ina.energiemWS()
                    #print(ina["address"]," voltage", ina["voltage"])
                self.totalEnergiemWS=totalEnergiemWS

    def stop(self):
        self.running=False
        self.join()
        AstraIna._AstraInaFetcher=None

    def setIna(self, ina):
        with self.listInalock:
            self.listIna.append(ina)

    def getTotalEnergiemWS(self)->float:
        """
        Return the sum of INA energie measurements in mWs.
        """
        return self.totalEnergiemWS


class AstraIna:
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
                         "bus_adc":INA219.ADC_128SAMP, "shunt_adc":INA219.ADC_128SAMP },
            "AstraDc2": {"ispwm":False, "busnum":1, "address": 0x44, "shunt_ohms": 0.01, "max_expected_amps": 6, "pin": 38, 
                         "bus_adc":INA219.ADC_128SAMP, "shunt_adc":INA219.ADC_128SAMP },
            "AstraDc3": {"ispwm":False, "busnum":1, "address": 0x46, "shunt_ohms": 0.01, "max_expected_amps": 6, "pin": 40, 
                         "bus_adc":INA219.ADC_128SAMP, "shunt_adc":INA219.ADC_128SAMP },
            "AstraPwm1": {"ispwm":True, "busnum":1, "address": 0x49, "shunt_ohms": 0.01, "max_expected_amps": 6, "chip":2, "pwm":1, 
                         "bus_adc":INA219.ADC_128SAMP, "shunt_adc":INA219.ADC_128SAMP },
            "AstraPwm2": {"ispwm":True, "busnum":1, "address": 0x4d, "shunt_ohms": 0.01, "max_expected_amps": 6, "chip":2, "pwm":2, 
                         "bus_adc":INA219.ADC_128SAMP, "shunt_adc":INA219.ADC_128SAMP },
            "AstOnStep": {"ispwm":True, "busnum":1, "address": 0x40, "shunt_ohms": 0.005, "max_expected_amps": 6, "chip":2, "pwm":2, 
                         "bus_adc":INA219.ADC_128SAMP, "shunt_adc":INA219.ADC_128SAMP }
    }

    @classmethod
    def getListNames(cls):
        return AstraIna.ina219_set.keys()

    @classmethod
    def exitAll(cls):
        AstraInaFetcher.exitAll()

    def __init__(self, shunt_ohms=-1, max_expected_amps=-1, busnum=-1, address=-1, name="", log_level=logging.ERROR):
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
        self.firstPing=True
        self.pingOk=True
        self._lasttimeS:float=0.0
        self._firsttime:float=0.0
        self._intPeriodS:float=0.0

        self._voltageV:float=0.0
        self._shuntVoltagemV:float=0.0
        self._currentmA:float=0.0
        self._powermW:float=0.0
        self._energiemWS:float=0.0
    
        self.ina219:INA219= None
        self.AstraInaFetcher:AstraInaFetcher=None

        if self.name == "":
            if shunt_ohms==-1 or max_expected_amps==-1 or busnum==-1 or address==-1:
                raise Exception("If name notspecified call AstraIna(shunt_ohms, max_expected_amps, busnum, address")
            # Temp fetcher
            self.ina219 = INA219(shunt_ohms=shunt_ohms, max_expected_amps=max_expected_amps, busnum=busnum, address=address, log_level=log_level)
            self.AstraInaFetcher = AstraInaFetcher.get_instance()
        else:
            if self.name in self.ina219_set:
                self.caract=self.ina219_set[name]
                self.ina219 = INA219(
                    shunt_ohms=self.caract["shunt_ohms"], 
                    max_expected_amps=self.caract["max_expected_amps"], 
                    busnum=self.caract["busnum"], 
                    address=self.caract["address"], 
                    log_level=log_level)
                # Temp fetcher
                self.AstraInaFetcher = AstraInaFetcher.get_instance()
                self.configure(bus_adc=self.caract["bus_adc"], shunt_adc=self.caract["shunt_adc"])
            else:
                raise Exception("Unkown AstraIna")

    def sendConfiguration(self):
        """
        As the INA may loose it's configuration, it is necessary to:
        1- Check if the ina is present.
        2- Send the configuration if it is present.
        Collect of the data shall be done through getDataFromIna 
        The method is not threadsafe and shall be called by a uniq thread.
        """
        pingOk=False
        self.configurationSend=False
        if self.ina219.ping():
            pingOk=True
            if self.firstPing:
                self.firstPing=False
                self._lasttimeS = time.perf_counter()
                self.firstttime= time.perf_counter()
        if pingOk:
            self.ina219.configure(
                voltage_range=self.voltage_range, 
                gain=self.gain, 
                bus_adc=self.bus_adc, 
                shunt_adc=self.shunt_adc)
            self.configurationSend = True            
        
    def getDataFromIna(self):
        """
        Do the INA iteraction.
        The user shall have called sendConfiguration each time before calling this method.
        Collects measures of the INA for publication.
        The method is not threadsafe and shall be called by a uniq thread.
        It is considered that the last measure OK is cummulated in the energy.
        """
        if self.configurationSend:
            curtimeS=time.perf_counter()
            deltatimeS=curtimeS-self._lasttimeS
            if not self.ina219.current_overflow():
                self._shuntVoltagemV = self.ina219.shunt_voltage()
                self._voltageV = float(self.ina219.voltage())
                self._currentmA = float(self.ina219.current())
                self._powermW = float(self.ina219.power())
            energiemWS=self._powermW * deltatimeS
            self._energiemWS += energiemWS
            self._lasttimeS=curtimeS
            self._intPeriodS=curtimeS-self.firstttime
    
    def configure(self, voltage_range=INA219.RANGE_16V, gain=INA219.GAIN_AUTO, bus_adc=INA219.ADC_12BIT, shunt_adc=INA219.ADC_12BIT):
        if self.configured:
            raise Exception("AstraIna already Configured")
        else:
            self.voltage_range=voltage_range
            self.gain=gain
            self.bus_adc=bus_adc
            self.shunt_adc=shunt_adc
            self.AstraInaFetcher.setIna(self)
            self.configured=True

    
    def getName(self)->str:
        return self.name

    def setName(self, name):
        self.name = name

    def voltageV(self)->float:
        """
        Return the last seen bus voltage in volts.
        """
        return self._voltageV

    def shuntVoltagemV(self)->float:
        """
        Return the last seen shunt voltage in millivolts.
        """
        return self._shuntVoltagemV

    def shuntVoltageV(self)->float:
        """
        Return the last seen shunt voltage in millivolts.
        """
        return self._shuntVoltagemV / 1000
    
    def currentmA(self)->float:
        """
        Return the bus current in milliamps.
        """
        return self._currentmA

    def currentA(self)->float:
        """
        Return the bus current in Amps.
        """
        return (self._currentmA/1000.0)
    
    def powermW(self)->float:
        """
        Return the bus power consumption in milliwatts.
        """
        return self._powermW

    def powerW(self)->float:
        """
        Return the bus power consumption in Watts.
        """
        return (self._powermW/1000.0)
    
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
    
    def getTotalEnergiemWS(self)->float:
        """
        Return the sum of INA energie measurements in mWs.
        """
        return self.AstraInaFetcher.getTotalEnergiemWS()    
    
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
            print("Energie=",ina219.getTotalEnergiemWS()/3600," mAh")
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
                current = ina219.current()
                power = ina219.power()
                energie = ina219.energie() / 60 / 60
                intPeriod=ina219.intPeriod()

                print(f"{name}: Shunt {shunt_voltage:+.3f}V, Bus {bus_voltage:+.3f}V Current: {current:+.3f}A, Power: {power:.3f}mW Energie: {energie:.3f}mWh intPeriod: {intPeriod:.3f}s")



