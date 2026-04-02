#!/usr/bin/env python3

import math
from typing import List, Optional

from lib.bme280_lib import BME280
from lib.astra_com_device import AstraComDevice


class AstraBmeFetcher(AstraComDevice):
    ROSEEUNAVAIL = -100
    TEMPUNAVAIL = 100
    _instance: Optional["AstraBmeFetcher"] = None

    @classmethod
    def get_instance(cls) -> "AstraBmeFetcher":
        if cls._instance is None:
            cls._instance = AstraBmeFetcher()
        return cls._instance

    def __init__(self) -> None:
        super().__init__(name="BME280")
        self.bmeSensor: Optional[BME280] = None
        self.bme_present = False
        self.bme_temperature = self.TEMPUNAVAIL
        self.bme_pressure = 0
        self.bme_humidity = 0
        self.bme_tempRosee = self.ROSEEUNAVAIL
        self.manual_humidity: Optional[float] = None

        self.DEW_POINT_FILTER_SIZE = 10
        self.DEW_POINT_MIN_CHANGE = 0.1
        self.TEMP_HUMIDITY_MIN_CHANGE = 0.05
        self.dewPointHistory: List[float] = []
        self.tempHistory: List[float] = []
        self.humidityHistory: List[float] = []
        self.filteredDewPoint = self.ROSEEUNAVAIL
        self.filteredTemp = 0.0
        self.filteredHumidity = 0.0

        try:
            self.bmeSensor = BME280()
            self.bmeSensor.loadCalibrationData()
            self.bmeSensor.configureNormalMode()
            self.bme_present = True
        except Exception:
            self.bmeSensor = None
            self.bme_present = False

    def _filter_value(self, new_value: float, history: List[float], min_change: float) -> float:
        if len(history) == 0:
            history.append(new_value)
            return new_value

        current_average = sum(history) / len(history)
        change = abs(new_value - current_average)

        if change < min_change and len(history) >= 3:
            return current_average

        history.append(new_value)
        if len(history) > self.DEW_POINT_FILTER_SIZE:
            history.pop(0)
        return sum(history) / len(history)

    def _filter_dew_point(self, new_dew_point: float) -> float:
        if len(self.dewPointHistory) == 0:
            self.dewPointHistory.append(new_dew_point)
            self.filteredDewPoint = new_dew_point
            return new_dew_point

        current_average = sum(self.dewPointHistory) / len(self.dewPointHistory)
        change = abs(new_dew_point - current_average)

        if change < self.DEW_POINT_MIN_CHANGE and len(self.dewPointHistory) >= 3:
            self.filteredDewPoint = current_average
            return current_average

        self.dewPointHistory.append(new_dew_point)
        if len(self.dewPointHistory) > self.DEW_POINT_FILTER_SIZE:
            self.dewPointHistory.pop(0)

        average = sum(self.dewPointHistory) / len(self.dewPointHistory)
        self.filteredDewPoint = average
        return average

    def getMeasurement(self, step: int, integrationDurationS: float) -> None:
        try:
            if self.bmeSensor is None:
                raise RuntimeError("BME280 not initialized")

            self.bme_temperature, self.bme_pressure, self.bme_humidity = self.bmeSensor.acquireDataNormalMode()
            self.bme_present = True

            self.filteredTemp = self._filter_value(
                self.bme_temperature, self.tempHistory, self.TEMP_HUMIDITY_MIN_CHANGE
            )

            humidity_to_use = self.bme_humidity
            if self.bme_humidity == 0 and self.manual_humidity is not None:
                humidity_to_use = self.manual_humidity

            self.filteredHumidity = self._filter_value(
                humidity_to_use, self.humidityHistory, self.TEMP_HUMIDITY_MIN_CHANGE
            )

            if humidity_to_use > 0:
                a = 17.27
                b = 237.7
                facteur = ((a * self.filteredTemp) / (b + self.filteredTemp)) + math.log(self.filteredHumidity / 100)
                dewPoint = b * facteur / (a - facteur)
                self.bme_tempRosee = self._filter_dew_point(dewPoint)
            else:
                self.dewPointHistory.clear()
                self.tempHistory.clear()
                self.humidityHistory.clear()
                self.filteredDewPoint = self.ROSEEUNAVAIL
                self.bme_tempRosee = self.ROSEEUNAVAIL

        except Exception:
            self.bme_present = False
            self.dewPointHistory.clear()
            self.tempHistory.clear()
            self.humidityHistory.clear()
            self.filteredDewPoint = self.ROSEEUNAVAIL
            self.bme_tempRosee = self.ROSEEUNAVAIL
            self.bme_temperature = self.TEMPUNAVAIL

    def get_bmeTemp(self) -> float:
        return self.bme_temperature

    def get_bmePressure(self) -> float:
        return self.bme_pressure

    def get_bmeHumidity(self) -> float:
        if self.bme_humidity == 0 and self.manual_humidity is not None:
            return self.manual_humidity
        return self.bme_humidity

    def get_bmeTempRosee(self) -> float:
        return self.bme_tempRosee

    def get_filteredTemp(self) -> float:
        return self.filteredTemp

    def get_filteredHumidity(self) -> float:
        return self.filteredHumidity

    def get_filteredDewPoint(self) -> float:
        return self.filteredDewPoint

    def isPresent_bme(self) -> bool:
        return self.bme_present

    def has_humidity_sensor(self) -> bool:
        return self.bme_humidity > 0

    def set_manual_humidity(self, humidity: float) -> None:
        self.manual_humidity = humidity