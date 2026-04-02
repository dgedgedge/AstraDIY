#!/usr/bin/env python3

"""Generic synchronized actor interface used by AstraComFetcher."""

from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from lib.astra_com_fetcher import AstraComFetcher


class AstraComActor:
    def __init__(self, name: Optional[str] = None) -> None:
        from lib.astra_com_fetcher import AstraComFetcher

        fetcher = AstraComFetcher.getInstance()
        self.name: str = self.__class__.__name__ if name is None else name
        self.cyclePeriodS = fetcher.getCyclePeriod()
        self.cycleStepCount = fetcher.getCycleStepCount()
        fetcher.addActor(self)
        self._registered_fetcher = fetcher

    def beforeMeasurements(self, step: int) -> None:
        """Called once per cycle, before measurement steps."""
        pass

    def getName(self) -> str:
        """Return the actor name."""
        return self.name

    def setName(self, name: str) -> None:
        """Update the actor name."""
        self.name = name

    def __str__(self) -> str:
        """Return the actor name for default string rendering."""
        return self.name

    def __format__(self, formatSpec: str) -> str:
        """Format the actor using its name by default."""
        return format(self.name, formatSpec)

    def onCycleConfigurationChanged(self, periodS: float, stepCount: int) -> None:
        """Called when the fetcher cycle period or step count changes."""
        self.cyclePeriodS = periodS
        self.cycleStepCount = stepCount
