#!/bin/env python3
import gps
import ntplib
import time
import threading
import re

from gps import gps, WATCH_ENABLE, WATCH_JSON 
import subprocess  

from statistics import mean, stdev
from collections import deque

class NTPMonitor:
    def __init__(self, ntpServer="127.0.0.1", maxSamples=20):
        self.ntpServer = ntpServer
        self.maxSamples = maxSamples
        self.client = ntplib.NTPClient()
        self.offsetsS = deque(maxlen=maxSamples)
        self.delaysS = deque(maxlen=maxSamples)
        self.rootDispersionS = deque(maxlen=maxSamples)
        self.precisionS = deque(maxlen=maxSamples)
        self.utcTimeS=0

    def fetchNtpData(self):
        response = self.client.request(self.ntpServer)
        self.offsetsS.append(response.offset)
        self.delaysS.append(response.delay)
        self.rootDispersionS.append(response.root_dispersion/65536.0)
        self.precisionS.append(2 ** response.precision)
        self.utcTimeS=response.tx_time

    def calculateRootDispersionS(self)->float:
        if len(self.rootDispersionS) > 1:
            return mean(self.rootDispersionS)
        return 0

    def calculatePrecisionS(self)->float:
        if len(self.precisionS) > 1:
            return mean(self.precisionS)
        return 0
    
    def calculateDispersionS(self)->float:
        if len(self.delaysS) > 1:
            return stdev(self.delaysS)
        return 0

    def calculateJitterS(self)->float:
        if len(self.offsetsS) > 1:
            return stdev(self.offsetsS)
        return 0
    
    def calculateUncertaintyS(self)->float:
        if not self.offsetsS or not self.delaysS:
            return float('inf')  # Retourne une incertitude infinie si aucune donnée n'est disponible
        return  abs(mean(self.offsetsS)) + self.calculateDispersionS() + self.calculateJitterS()

    def printAll(self):
        print(f"NTP Date {time.ctime(self.utcTimeS)}")
        print(f"NTP calculateRootDispersionS {self.calculateRootDispersionS()*1000.0:.3e} ms")
        print(f"NTP calculatePrecisionS {self.calculatePrecisionS()*1000.0:.3e} ms")
        print(f"NTP calculateDispersionS {self.calculateDispersionS()*1000.0:.3e} ms")
        print(f"NTP calculateJitterS {self.calculateJitterS()*1000.0:.3e} ms")
        print(f"NTP Total precicion {self.calculateUncertaintyS()*1000.0*1000.0:.1f} us")





class AstraGps(threading.Thread):
    _AstraGps=None

    def __init__(self, fetchPeriodS:float=0.5):
        super().__init__()
        self.running = True
        self.fetchPeriodS=fetchPeriodS

        # Gps Data
        self.ppsSignal = 0
        # Fix Mode
        self.fixMode=0
        self.syncState = "No Fix"
        # Time
        self.timeStamp=0
        self.lat=0
        self.long=0
        self.alt=0

    
        self.dispersionuS:float = 0
        self.precisionuS = 0
        self.current_time = 0
        self.time_error_margin = 0

        self.ntpMonitor:NTPMonitor = NTPMonitor()

    @classmethod
    def get_instance(cls):
        if cls._AstraGps is None:
            cls._AstraGps = AstraGps()
            cls._AstraGps.start()
        return cls._AstraGps

    @classmethod
    def exitAll(cls):
        if not(cls._AstraGps is None):
            cls._AstraGps.stop()


    def gpsGetPosition(self):
        return self.lat, self.long, self.alt
    
    def gpsCountPPS(self)->int:
        return self.ppsSignal
    
    def gpsSyncState(self)->str:
        return self.syncState

    def gpsTimeStamp(self)->int:
        return self.timeStamp
    
    def printAll(self):
        print(f"GPS PPS Signal: {self.ppsSignal}")  
        print(f"GPS Sync State: {self.fixMode} {self.syncState}") 
        print(f"GPS TimeStamp: {self.timeStamp}") 
        print(f"GPS Position: {self.lat:.3f} {self.long:.3f} {self.alt}") 
    
        self.ntpMonitor.printAll()
 
        print(f"System Time: {self.current_time} +/- {self.time_error_margin*1000.0:.3e} ms")

    def run(self):
        # Set up the GPS session in streaming mode
        session = gps(mode=WATCH_ENABLE | WATCH_JSON)

        while self.running:
            try:
                # Wait for the next GPS report (blocking call)
                report = session.next()
                # Handle different types of GPS reports
                if report['class'] == 'TPV':  # Time-Position-Velocity report
                    self.fixMode = getattr(report, 'mode', 0)  # 0 = No fix, 2 = 2D fix, 3 = 3D fix
                    time_stamp = getattr(report, 'time', 'No time')
                    lat = getattr(report, 'lat', None)
                    lon = getattr(report, 'lon', None)
                    alt = getattr(report, 'alt', None)
                    if self.fixMode <= 0:
                        self.syncState = "No Fix"
                    else:
                        self.syncState = f"{self.fixMode}D"
                    if self.fixMode >= 2:  # At least a 2D fix is required
                        self.timeStamp = time_stamp
                        self.lat=lat
                        self.long=lon
                        self.alt=alt

                if report['class'] == 'PPS':
                    self.ppsSignal=(self.ppsSignal)%10+1

            except KeyError:
                # Ignore missing keys if no GPS data is available
                pass
            except KeyboardInterrupt:
                print("Exiting GPS collection.")
                break
            except Exception as e:
                print(f"Error: {e}")

            # ntp lib
            self.ntpMonitor.fetchNtpData()
            
            # Check if chronyc is available  
            chronyc_available = False  
            try:  
                subprocess.check_output("chronyc --version", shell=True)  
                chronyc_available = True  
            except subprocess.CalledProcessError:  
                pass  
            
            if chronyc_available:
                # Run `chronyc tracking` to get time and error margin
                result = subprocess.run(["chronyc", "tracking"], capture_output=True, text=True, check=True)
                output = result.stdout

                # Extract Reference time and Root dispersion (error margin)
                ref_time_match = re.search(r"Ref.* time[^:]*:\s*(.+)", output)
                root_disp_match = re.search(r"Root dispersion\s*:\s*([\d.]+)\s*seconds", output)
                self.current_time = ref_time_match.group(1) if ref_time_match else "Unknown"
                self.time_error_margin = float(root_disp_match.group(1)) if root_disp_match else None

            time.sleep(self.fetchPeriodS)




if __name__ == "__main__":

    astraGps=AstraGps().get_instance()
    while True:
        astraGps.printAll()
        time.sleep(1)
    exit(0)

