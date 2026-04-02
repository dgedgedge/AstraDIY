#!/usr/bin/env python3

"""Generic communication device interface used by AstraComFetcher."""

from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from lib.astra_com_fetcher import AstraComFetcher


class AstraComDevice:
    def __init__(self, eachStep: bool = False, name: Optional[str] = None) -> None:
        self._registered_fetcher: Optional["AstraComFetcher"] = None
        self.cyclePeriodS: Optional[float] = None
        self.cycleStepCount: Optional[int] = None
        self.name: str = self.__class__.__name__ if name is None else name
        self.register_to_fetcher(eachStep=eachStep)

    def register_to_fetcher(self, fetcher: Optional["AstraComFetcher"] = None, eachStep: bool = False) -> None:
        """Register device into a fetcher once."""
        if self._registered_fetcher is not None:
            return

        if fetcher is None:
            from lib.astra_com_fetcher import AstraComFetcher

            fetcher = AstraComFetcher.getInstance()

        self.cyclePeriodS = fetcher.getCyclePeriod()
        self.cycleStepCount = fetcher.getCycleStepCount()
        fetcher.addDevice(self, eachStep=eachStep)
        self._registered_fetcher = fetcher

    def startMeasurement(self, step: int, integrationDurationS: float) -> None:
        """Optional measurement preparation step before data acquisition."""
        pass

    def getMeasurement(self, step: int, integrationDurationS: float) -> None:
        """Acquire fresh measurement data from the device."""
        raise NotImplementedError("getMeasurement must be implemented by subclasses")

    def getName(self) -> str:
        """Return the device name."""
        return self.name

    def setName(self, name: str) -> None:
        """Update the device name."""
        self.name = name

    def __str__(self) -> str:
        """Return the device name for default string rendering."""
        return self.name

    def __format__(self, formatSpec: str) -> str:
        """Format the device using its name by default."""
        return format(self.name, formatSpec)

    def onCycleConfigurationChanged(self, periodS: float, stepCount: int) -> None:
        """Called when the fetcher cycle period or step count changes."""
        self.cyclePeriodS = periodS
        self.cycleStepCount = stepCount
