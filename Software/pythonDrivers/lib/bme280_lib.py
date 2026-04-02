#!/usr/bin/env python3

"""Lecture du capteur BME280 via I2C."""

import time
from ctypes import c_short
from typing import Optional, Sequence, Tuple

try:
  import smbus  # type: ignore[import-not-found]
except ImportError:
  smbus = None


class BME280:
  """Wrapper objet pour capteur BME280.

  Par defaut, le capteur est adresse en I2C sur 0x76.
  """

  DEFAULT_DEVICE = 0x76

  def __init__(self, addr: int = DEFAULT_DEVICE, busId: int = 1) -> None:
    """Initialise le capteur et ouvre le bus I2C.

    Args:
      addr: Adresse I2C du BME280.
      busId: Numero du bus I2C (1 sur Raspberry Pi moderne).
    """
    if smbus is None:
      raise RuntimeError("Le module python smbus est requis pour utiliser BME280")

    self.addr = addr
    self.busId = busId
    self.bus = smbus.SMBus(busId)

    self._calibrationLoaded = False
    self._normalModeConfigured = False
    self._normalModeWaitTimeS = 0.0

    self._digT1: int = 0
    self._digT2: int = 0
    self._digT3: int = 0

    self._digP1: int = 0
    self._digP2: int = 0
    self._digP3: int = 0
    self._digP4: int = 0
    self._digP5: int = 0
    self._digP6: int = 0
    self._digP7: int = 0
    self._digP8: int = 0
    self._digP9: int = 0

    self._digH1: int = 0
    self._digH2: int = 0
    self._digH3: int = 0
    self._digH4: int = 0
    self._digH5: int = 0
    self._digH6: int = 0

  def getShort(self, data: Sequence[int], index: int) -> int:
    """Retourne 2 octets signes (16 bits)."""
    return c_short((data[index + 1] << 8) + data[index]).value

  def getUShort(self, data: Sequence[int], index: int) -> int:
    """Retourne 2 octets non signes (16 bits)."""
    return (data[index + 1] << 8) + data[index]

  def getChar(self, data: Sequence[int], index: int) -> int:
    """Retourne 1 octet signe."""
    result = data[index]
    if result > 127:
      result -= 256
    return result

  def getUChar(self, data: Sequence[int], index: int) -> int:
    """Retourne 1 octet non signe."""
    return data[index] & 0xFF

  def readBME280ID(self, addr: Optional[int] = None) -> Tuple[int, int]:
    """Lit l'identifiant du capteur.

    Args:
      addr: Adresse I2C optionnelle (sinon adresse de l'objet).

    Returns:
      Tuple (chipId, chipVersion).
    """
    regId = 0xD0
    deviceAddr = self.addr if addr is None else addr
    chipId, chipVersion = self.bus.read_i2c_block_data(deviceAddr, regId, 2)
    return chipId, chipVersion

  def loadCalibrationData(self, addr: Optional[int] = None) -> None:
    """Lit les coefficients de calibration depuis l'EEPROM du BME280.

    Les valeurs dig_* sont conservees dans des attributs de l'objet.
    """
    deviceAddr = self.addr if addr is None else addr

    cal1 = self.bus.read_i2c_block_data(deviceAddr, 0x88, 24)
    cal2 = self.bus.read_i2c_block_data(deviceAddr, 0xA1, 1)
    cal3 = self.bus.read_i2c_block_data(deviceAddr, 0xE1, 7)

    self._digT1 = self.getUShort(cal1, 0)
    self._digT2 = self.getShort(cal1, 2)
    self._digT3 = self.getShort(cal1, 4)

    self._digP1 = self.getUShort(cal1, 6)
    self._digP2 = self.getShort(cal1, 8)
    self._digP3 = self.getShort(cal1, 10)
    self._digP4 = self.getShort(cal1, 12)
    self._digP5 = self.getShort(cal1, 14)
    self._digP6 = self.getShort(cal1, 16)
    self._digP7 = self.getShort(cal1, 18)
    self._digP8 = self.getShort(cal1, 20)
    self._digP9 = self.getShort(cal1, 22)

    self._digH1 = self.getUChar(cal2, 0)
    self._digH2 = self.getShort(cal3, 0)
    self._digH3 = self.getUChar(cal3, 2)

    digH4 = self.getChar(cal3, 3)
    digH4 = (digH4 << 24) >> 20
    self._digH4 = digH4 | (self.getChar(cal3, 4) & 0x0F)

    digH5 = self.getChar(cal3, 5)
    digH5 = (digH5 << 24) >> 20
    self._digH5 = digH5 | (self.getUChar(cal3, 4) >> 4 & 0x0F)

    self._digH6 = self.getChar(cal3, 6)
    self._calibrationLoaded = True

  def configureNormalMode(
    self,
    addr: Optional[int] = None,
    oversampleTemp: int = 2,
    oversamplePres: int = 2,
    oversampleHum: int = 2,
    standbyCode: int = 5,
    filterCode: int = 0,
  ) -> None:
    """Configure le capteur en mode normal (acquisition continue).

    Conformement a la datasheet BME280:
    1. ecriture de ctrl_hum (0xF2)
    2. ecriture de config (0xF5)
    3. ecriture de ctrl_meas (0xF4) avec mode=0b11 (normal)
    """
    deviceAddr = self.addr if addr is None else addr

    regControlHum = 0xF2
    regConfig = 0xF5
    regControlMeas = 0xF4
    normalMode = 0x03

    oversampleTemp = max(0, min(5, oversampleTemp))
    oversamplePres = max(0, min(5, oversamplePres))
    oversampleHum = max(0, min(5, oversampleHum))
    standbyCode = max(0, min(7, standbyCode))
    filterCode = max(0, min(4, filterCode))

    self.bus.write_byte_data(deviceAddr, regControlHum, oversampleHum)

    config = (standbyCode << 5) | (filterCode << 2)
    self.bus.write_byte_data(deviceAddr, regConfig, config)

    control = (oversampleTemp << 5) | (oversamplePres << 2) | normalMode
    self.bus.write_byte_data(deviceAddr, regControlMeas, control)

    waitTimeMs = (
      1.25
      + (2.3 * oversampleTemp)
      + ((2.3 * oversamplePres) + 0.575)
      + ((2.3 * oversampleHum) + 0.575)
    )
    self._normalModeWaitTimeS = waitTimeMs / 1000.0
    self._normalModeConfigured = True
    time.sleep(self._normalModeWaitTimeS)

  def acquireDataNormalMode(self, addr: Optional[int] = None) -> Tuple[float, float, float]:
    """Lit temperature (C), pression (hPa) et humidite (%) en mode normal."""
    deviceAddr = self.addr if addr is None else addr

    if not self._calibrationLoaded:
      self.loadCalibrationData(deviceAddr)

    if not self._normalModeConfigured:
      self.configureNormalMode(deviceAddr)

    regData = 0xF7
    data = self.bus.read_i2c_block_data(deviceAddr, regData, 8)
    presRaw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4)
    tempRaw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4)
    humRaw = (data[6] << 8) | data[7]

    var1 = ((((tempRaw >> 3) - (self._digT1 << 1))) * self._digT2) >> 11
    var2 = (((((tempRaw >> 4) - self._digT1) * ((tempRaw >> 4) - self._digT1)) >> 12) * self._digT3) >> 14
    tFine = var1 + var2
    temperature = float(((tFine * 5) + 128) >> 8)

    var1p = tFine / 2.0 - 64000.0
    var2p = var1p * var1p * self._digP6 / 32768.0
    var2p = var2p + var1p * self._digP5 * 2.0
    var2p = var2p / 4.0 + self._digP4 * 65536.0
    var1p = (self._digP3 * var1p * var1p / 524288.0 + self._digP2 * var1p) / 524288.0
    var1p = (1.0 + var1p / 32768.0) * self._digP1
    if var1p == 0:
      pressure = 0.0
    else:
      pressure = 1048576.0 - presRaw
      pressure = ((pressure - var2p / 4096.0) * 6250.0) / var1p
      var1p2 = self._digP9 * pressure * pressure / 2147483648.0
      var2p2 = pressure * self._digP8 / 32768.0
      pressure = pressure + (var1p2 + var2p2 + self._digP7) / 16.0

    humidity = tFine - 76800.0
    humidity = (
      (humRaw - (self._digH4 * 64.0 + self._digH5 / 16384.0 * humidity))
      * (
        self._digH2
        / 65536.0
        * (1.0 + self._digH6 / 67108864.0 * humidity * (1.0 + self._digH3 / 67108864.0 * humidity))
      )
    )
    humidity = humidity * (1.0 - self._digH1 * humidity / 524288.0)
    if humidity > 100:
      humidity = 100.0
    elif humidity < 0:
      humidity = 0.0

    return temperature / 100.0, pressure / 100.0, humidity

  def readBME280All(self, addr: Optional[int] = None) -> Tuple[float, float, float]:
    """Compatibilite API historique: acquisition complete en mode normal."""
    return self.acquireDataNormalMode(addr)


_defaultSensor: Optional[BME280] = None


def _getDefaultSensor() -> BME280:
  """Retourne l'instance BME280 par defaut, creee a la demande."""
  global _defaultSensor
  if _defaultSensor is None:
    _defaultSensor = BME280()
  return _defaultSensor


def getShort(data: Sequence[int], index: int) -> int:
  """Compatibilite: helper module-level vers l'instance par defaut."""
  return _getDefaultSensor().getShort(data, index)


def getUShort(data: Sequence[int], index: int) -> int:
  """Compatibilite: helper module-level vers l'instance par defaut."""
  return _getDefaultSensor().getUShort(data, index)


def getChar(data: Sequence[int], index: int) -> int:
  """Compatibilite: helper module-level vers l'instance par defaut."""
  return _getDefaultSensor().getChar(data, index)


def getUChar(data: Sequence[int], index: int) -> int:
  """Compatibilite: helper module-level vers l'instance par defaut."""
  return _getDefaultSensor().getUChar(data, index)


def readBME280ID(addr: int = BME280.DEFAULT_DEVICE) -> Tuple[int, int]:
  """Compatibilite: lecture ID avec API historique."""
  return _getDefaultSensor().readBME280ID(addr)


def readBME280All(addr: int = BME280.DEFAULT_DEVICE) -> Tuple[float, float, float]:
  """Compatibilite: lecture complete avec API historique."""
  return _getDefaultSensor().readBME280All(addr)


def main() -> None:
  """Petit test local de lecture capteur."""
  sensor = _getDefaultSensor()
  sensor.loadCalibrationData()
  sensor.configureNormalMode()

  chipId, chipVersion = sensor.readBME280ID()
  print("Chip ID     :", chipId)
  print("Version     :", chipVersion)

  temperature, pressure, humidity = sensor.readBME280All()
  print("Temperature :", temperature, "C")
  print("Pressure    :", pressure, "hPa")
  print("Humidity    :", humidity, "%")


if __name__ == "__main__":
  main()

