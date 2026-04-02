#!/usr/bin/env python3

"""Step-based GPIO PWM actor synchronized on AstraComFetcher cycles."""

import importlib
from typing import Any, Optional

from lib.astra_com_actor import AstraComActor


class AstraStepPwmActor(AstraComActor):
    """Drive GPIO 18 or 13 with a duty cycle expressed in logical steps."""

    gpioToPin = {13: 33, 18: 12}

    @staticmethod
    def _getGpiodModule() -> Any:
        """Load gpiod lazily because it is only available on the target system."""
        try:
            return importlib.import_module("gpiod")
        except ImportError as exc:
            raise RuntimeError("The python gpiod module is required on the target Raspberry Pi") from exc

    def __init__(self, gpio: int, stepPercent: float = 0.0, name: Optional[str] = None) -> None:
        """Create a synchronized step-based PWM actor on GPIO 18 or 13."""
        self.gpio = gpio
        self._line = self._openGpioLine(gpio)
        self._stepPercent = stepPercent
        self.highStepCount = 0
        self.highStepIndexes: set[int] = set()
        actorName = f"AstraStepPwmActor(GPIO{gpio})" if name is None else name
        super().__init__(name=actorName)
        self._updateHighStepCount()

    @classmethod
    def _openGpioLine(cls, gpio: int):
        """Resolve and request the GPIO output line."""
        gpiod = cls._getGpiodModule()

        if gpio not in cls.gpioToPin:
            raise ValueError(f"Unsupported GPIO {gpio}. Only GPIO 13 and GPIO 18 are allowed")

        line = None
        for lineName in (f"GPIO{gpio}", f"PIN{cls.gpioToPin[gpio]}"):
            line = gpiod.find_line(lineName)
            if line is not None:
                break

        if line is None:
            raise RuntimeError(f"GPIO {gpio} not found")

        try:
            line.request(consumer="AstraStepPwmActor")
        except OSError as exc:
            consumer = line.consumer()
            if consumer:
                raise RuntimeError(
                    f"GPIO {line.name()} is already used by '{consumer}'. Close the other process first"
                ) from exc
            raise RuntimeError(f"GPIO {line.name()} is already used") from exc

        if line.direction() != line.DIRECTION_OUTPUT:
            line.set_direction_output()

        return line

    def _updateHighStepCount(self) -> None:
        """Update the cached number of logical high steps."""
        stepCount = self.cycleStepCount if self.cycleStepCount is not None else 1
        highStepCount = round(stepCount * self._stepPercent / 100.0)
        self.highStepCount = max(0, min(highStepCount, stepCount))

        if self.highStepCount == 0:
            self.highStepIndexes = set()
            return

        if self.highStepCount >= stepCount:
            self.highStepIndexes = set(range(stepCount))
            return

        # Spread HIGH steps over the full cycle: e.g. 2/10 -> {0, 5}.
        distributedSteps = {
            int(round(index * stepCount / self.highStepCount)) % stepCount
            for index in range(self.highStepCount)
        }

        # Ensure we keep exactly highStepCount unique steps.
        if len(distributedSteps) < self.highStepCount:
            candidate = 0
            while len(distributedSteps) < self.highStepCount and candidate < stepCount:
                if candidate not in distributedSteps:
                    distributedSteps.add(candidate)
                candidate += 1

        self.highStepIndexes = distributedSteps

    def setStepPercent(self, stepPercent: float) -> None:
        """Program the high duration as a percentage of the logical step count."""
        if stepPercent < 0 or stepPercent > 100:
            raise ValueError("stepPercent must be between 0 and 100")

        self._stepPercent = stepPercent
        self._updateHighStepCount()

    def getStepPercent(self) -> float:
        """Return the high duration as a percentage of the logical step count."""
        return self._stepPercent

    def isHigh(self) -> bool:
        """Return the current GPIO output state."""
        return self._line.get_value() != 0

    def beforeMeasurements(self, step: int) -> None:
        """Update the GPIO state at the beginning of each logical step."""
        self._line.set_value(1 if step in self.highStepIndexes else 0)

    def onCycleConfigurationChanged(self, periodS: float, stepCount: int) -> None:
        """Re-apply the programmed percentage when cycle settings change."""
        super().onCycleConfigurationChanged(periodS, stepCount)
        self._updateHighStepCount()

    def __str__(self) -> str:
        """Return the actor name and GPIO for default string rendering."""
        return f"{self.getName()} gpio={self.gpio}"

    def __format__(self, formatSpec: str) -> str:
        """Format the actor using its name and GPIO by default."""
        return format(str(self), formatSpec)

    def close(self) -> None:
        """Force the GPIO low and release the line."""
        self._line.set_value(0)
        release = getattr(self._line, "release", None)
        if callable(release):
            release()
