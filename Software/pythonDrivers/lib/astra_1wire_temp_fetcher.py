#!/usr/bin/env python3

import glob
import os
import threading
import time
from typing import Dict, List, Optional


class Astra1WireTempFetcher(threading.Thread):
    """Thread indépendant de lecture round-robin des capteurs 1-wire DS18B20."""

    TEMPUNAVAIL = 100
    CYCLE_PERIOD_S = 1.0

    _instance: Optional["Astra1WireTempFetcher"] = None

    @classmethod
    def get_instance(cls) -> "Astra1WireTempFetcher":
        """Retourne l'instance singleton, la crée et démarre le thread si nécessaire."""
        if cls._instance is None:
            cls._instance = Astra1WireTempFetcher()
            cls._instance.start()
        return cls._instance

    def __init__(self) -> None:
        super().__init__(daemon=True, name="Astra1WireTempFetcher")
        self.lock = threading.Lock()
        self.tableTemp: Dict[str, Dict[str, object]] = {}
        self._nextSensorIndex = 0
        self._tempNames: List[str] = []
        self._tempNameCount = 0
        self._stopEvent = threading.Event()

    def stop(self) -> None:
        """Arrête le thread d'acquisition."""
        self._stopEvent.set()

    def run(self) -> None:
        """Boucle principale : mise à jour de la liste puis lecture d'un capteur par cycle."""
        while not self._stopEvent.is_set():
            cycleStart = time.monotonic()

            try:
                self._update_templist()
                self._readOneSensor()
            except Exception:
                pass

            elapsed = time.monotonic() - cycleStart
            remaining = self.CYCLE_PERIOD_S - elapsed
            if remaining > 0:
                self._stopEvent.wait(timeout=remaining)

    def _update_templist(self) -> None:
        hasChange = False
        for path in glob.glob('/sys/bus/w1/devices/28*'):
            name = os.path.basename(path)
            filename = path + "/w1_slave"
            if os.access(filename, os.R_OK):
                with self.lock:
                    if name not in self.tableTemp:
                        self.tableTemp[name] = {"val": 0, "file": filename, "failCount": 0}
                        hasChange = True

        with self.lock:
            if hasChange or self._tempNameCount == 0:
                self._tempNames = list(self.tableTemp.keys())
                self._tempNameCount = len(self._tempNames)
                if self._tempNameCount == 0:
                    self._nextSensorIndex = 0
                elif self._nextSensorIndex >= self._tempNameCount:
                    self._nextSensorIndex = 0

    def _read_temp_lines(self, path: str) -> List[str]:
        with open(path, 'r') as f:
            return f.readlines()

    def _readOneSensor(self) -> None:
        """Lit un capteur en round-robin et met à jour tableTemp."""
        with self.lock:
            if self._tempNameCount == 0:
                return

            if self._nextSensorIndex >= self._tempNameCount:
                self._nextSensorIndex = 0

            tempName = self._tempNames[self._nextSensorIndex]
            tempFile = str(self.tableTemp[tempName]["file"])
            self._nextSensorIndex = (self._nextSensorIndex + 1) % self._tempNameCount

        returnval = 998
        try:
            lines = self._read_temp_lines(tempFile)
            if len(lines) == 2 and lines[0].strip().endswith('YES'):
                equals_pos = lines[1].find('t=')
                if equals_pos != -1:
                    temp = lines[1][equals_pos + 2:]
                    returnval = float(temp) / 1000
        except Exception:
            pass

        with self.lock:
            if tempName in self.tableTemp:
                if returnval != 998:
                    self.tableTemp[tempName]["val"] = returnval
                    self.tableTemp[tempName]["failCount"] = 0
                else:
                    self.tableTemp[tempName]["val"] = self.TEMPUNAVAIL
                    previousFailCount = int(self.tableTemp[tempName].get("failCount", 0))
                    self.tableTemp[tempName]["failCount"] = previousFailCount + 1

    def get_listTemp(self) -> List[str]:
        with self.lock:
            return list(self.tableTemp.keys())

    def get_temp(self, tempname: str) -> float:
        with self.lock:
            if tempname in self.tableTemp:
                return float(self.tableTemp[tempname]["val"])
        return self.TEMPUNAVAIL

    def get_failCount(self, tempname: str) -> int:
        with self.lock:
            if tempname in self.tableTemp:
                return int(self.tableTemp[tempname].get("failCount", 0))
        return 0

    def get_snapshot(self) -> Dict[str, Dict[str, object]]:
        with self.lock:
            return {name: values.copy() for name, values in self.tableTemp.items()}