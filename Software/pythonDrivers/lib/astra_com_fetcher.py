#!/usr/bin/env python3

"""Common communication fetcher for INA and BME sensors."""

import ctypes
import os
import threading
import time
from typing import List, Optional

from lib.astra_com_actor import AstraComActor
from lib.astra_com_device import AstraComDevice


class AstraCycleTimer:
    """Periodic cycle timer backed by timerfd when available."""

    def __init__(self, periodS: float) -> None:
        """Initialize the timer with a cycle period in seconds."""
        self.periodS = periodS
        self._timerFd: Optional[int] = None
        self._nextCycleDeadline: Optional[float] = None
        self._stopEvent = threading.Event()
        self._timerLock = threading.Lock()

    @staticmethod
    def _floatToTimespec(seconds: float) -> tuple[int, int]:
        """Convert fractional seconds to a timespec tuple."""
        sec = int(seconds)
        nsec = int((seconds - sec) * 1_000_000_000)
        return sec, nsec

    def _setupCycleTimer(self) -> bool:
        """Configure the underlying Linux periodic timer."""
        # Linux timerfd provides an OS-driven periodic wakeup.
        libc = ctypes.CDLL(None, use_errno=True)

        class Timespec(ctypes.Structure):
            _fields_ = [("tv_sec", ctypes.c_long), ("tv_nsec", ctypes.c_long)]

        class Itimerspec(ctypes.Structure):
            _fields_ = [("it_interval", Timespec), ("it_value", Timespec)]

        timerfd_create = getattr(libc, "timerfd_create", None)
        timerfd_settime = getattr(libc, "timerfd_settime", None)
        if timerfd_create is None or timerfd_settime is None:
            return False

        timerfd_create.argtypes = [ctypes.c_int, ctypes.c_int]
        timerfd_create.restype = ctypes.c_int
        timerfd_settime.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(Itimerspec), ctypes.c_void_p]
        timerfd_settime.restype = ctypes.c_int

        clockMonotonic = 1
        tfdCloexec = getattr(os, "O_CLOEXEC", 0)

        fd = timerfd_create(clockMonotonic, tfdCloexec)
        if fd < 0:
            return False

        sec, nsec = self._floatToTimespec(self.periodS)
        spec = Itimerspec(Timespec(sec, nsec), Timespec(sec, nsec))
        if timerfd_settime(fd, 0, ctypes.byref(spec), None) != 0:
            os.close(fd)
            return False

        self._timerFd = fd
        return True

    def setPeriod(self, periodS: float) -> None:
        """Update the timer period and reconfigure the running timer if needed."""
        if periodS <= 0:
            raise ValueError("periodS must be greater than 0")

        with self._timerLock:
            self.periodS = periodS
            self._nextCycleDeadline = None
            timerFd = self._timerFd

        if timerFd is not None:
            self.close()
            self._setupCycleTimer()

    def start(self) -> None:
        """Start a new cycle sequence."""
        self._stopEvent.clear()
        with self._timerLock:
            self._nextCycleDeadline = None
        self._setupCycleTimer()

    def waitNextCycle(self) -> bool:
        """Block until the next cycle or until a stop is requested."""
        if self._stopEvent.is_set():
            return False

        with self._timerLock:
            timerFd = self._timerFd

        if timerFd is None:
            now = time.monotonic()
            with self._timerLock:
                if self._nextCycleDeadline is None:
                    self._nextCycleDeadline = now + self.periodS
                nextCycleDeadline = self._nextCycleDeadline
                periodS = self.periodS

            remaining = nextCycleDeadline - now
            if remaining > 0:
                # Keep millisecond-compatible precision in fallback mode.
                self._stopEvent.wait(max(remaining, 0.001))
                if self._stopEvent.is_set():
                    return False

            now = time.monotonic()
            with self._timerLock:
                while self._nextCycleDeadline is not None and self._nextCycleDeadline <= now:
                    self._nextCycleDeadline += periodS
            return True

        try:
            os.read(timerFd, 8)
            return not self._stopEvent.is_set()
        except (InterruptedError, BlockingIOError):
            return not self._stopEvent.is_set()
        except OSError:
            if self._stopEvent.is_set():
                return False

            self.close()
            return self.waitNextCycle()

    def requestStop(self) -> None:
        """Stop the timer and unblock any pending wait."""
        self._stopEvent.set()
        self.close()

    def getPeriod(self) -> float:
        """Return the configured cycle period in seconds."""
        with self._timerLock:
            return self.periodS

    def close(self) -> None:
        """Release the timer file descriptor if it exists."""
        with self._timerLock:
            timerFd = self._timerFd
            self._timerFd = None

        if timerFd is not None:
            try:
                os.close(timerFd)
            except OSError:
                pass


class AstraComFetcher(threading.Thread):
    """Synchronize periodic measurements across registered devices and actors."""
    ACTIVATION_TOLERANCE_RATIO = 0.10
    PHASE_DELAY_S = 0.001
    MAX_CONVERSION_TIME_S = 0.001
    CYCLE_PERIOD_S = 0.3
    CYCLE_STEP_COUNT = 10
    _instance: Optional["AstraComFetcher"] = None

    def __init__(self) -> None:
        """Create the singleton fetcher thread state."""
        super().__init__()
        self.running = True
        self._hasSharedUpdate = False

        self._cyclePeriodS = self.CYCLE_PERIOD_S
        self._cycleTimer = AstraCycleTimer(self._cyclePeriodS)
        self._cycleStepCount = self.CYCLE_STEP_COUNT
        self._lastActivationTs: Optional[float] = None

        self.devicesLock = threading.Lock()
        self.eachStepDevices: List[AstraComDevice] = []
        self.stepIndexedDevices: List[List[AstraComDevice]] = [
            [] for _ in range(self._cycleStepCount)
        ]

        self.actorsLock = threading.Lock()
        self.actors: List[AstraComActor] = []

    def _getCycleConfiguration(self) -> tuple[float, int]:
        """Return the current cycle configuration."""
        return self._cyclePeriodS, self._cycleStepCount

    def _notifyCycleConfigurationChanged(self) -> None:
        """Notify registered actors and devices about cycle configuration changes."""
        periodS, stepCount = self._getCycleConfiguration()

        with self.actorsLock:
            actors = list(self.actors)
        with self.devicesLock:
            self._reassignStepIndexedDevicesNoLock()

            stepDevices = [device for bucket in self.stepIndexedDevices for device in bucket]
            devices = list(self.eachStepDevices) + stepDevices

        for actor in actors:
            try:
                actor.onCycleConfigurationChanged(periodS, stepCount)
            except Exception:
                pass

        for device in devices:
            try:
                device.onCycleConfigurationChanged(periodS, stepCount)
            except Exception:
                pass
        print(f"[DEBUG AstraComFetcher] Cycle configuration changed: period={periodS:.3f}s steps={stepCount} devices={len(devices)} actors={len(actors)}")
        print(f"[DEBUG AstraComFetcher] Each-step devices: {[str(device) for device in self.eachStepDevices]}")
        for index, bucket in enumerate(self.stepIndexedDevices):
            print(f"[DEBUG AstraComFetcher] Step {index} devices: {[str(device) for device in bucket]}")
        print(f"[DEBUG AstraComFetcher] Actors: {[str(actor) for actor in actors]}")

    @classmethod
    def getInstance(cls) -> "AstraComFetcher":
        """Return the shared fetcher instance, creating and starting it if needed."""
        if cls._instance is None:
            cls._instance = AstraComFetcher()
            cls._instance.start()
        return cls._instance

    @classmethod
    def exitAll(cls) -> None:
        """Stop the shared fetcher instance if it is running."""
        if cls._instance is not None:
            cls._instance.stop()

    def addDevice(self, device: AstraComDevice, eachStep: bool = False) -> None:
        """Register a device either for each step or for one distributed step."""
        self._hasSharedUpdate = True
        with self.devicesLock:
            if eachStep:
                self.eachStepDevices.append(device)
            else:
                # _notifyCycleConfigurationChanged() redistributes all buckets.
                # Temporary insertion point can be any valid step.
                self.stepIndexedDevices[0].append(device)
        self._notifyCycleConfigurationChanged()

    def addActor(self, actor: AstraComActor) -> None:
        """Register an actor notified before and after each cycle."""
        self._hasSharedUpdate = True
        with self.actorsLock:
            self.actors.append(actor)
        self._notifyCycleConfigurationChanged()

    def getCycleTimer(self) -> AstraCycleTimer:
        """Expose the cycle timer used by the fetcher."""
        return self._cycleTimer

    def getCyclePeriod(self) -> float:
        """Return the current cycle period in seconds."""
        return self._cyclePeriodS

    def setCyclePeriod(self, periodS: float) -> None:
        """Set the cycle period used by the fetcher."""
        if periodS <= 0:
            raise ValueError("periodS must be greater than 0")

        self._hasSharedUpdate = True
        self._cyclePeriodS = periodS
        self._cycleTimer.setPeriod(periodS)
        self._notifyCycleConfigurationChanged()

    def getCycleStepCount(self) -> int:
        """Return the number of steps in the cycle sequence."""
        return self._cycleStepCount

    def setCycleStepCount(self, stepCount: int) -> None:
        """Set the number of logical steps in the cycle sequence."""
        if stepCount <= 0:
            raise ValueError("stepCount must be greater than 0")

        self._hasSharedUpdate = True
        self._cycleStepCount = stepCount
        self._notifyCycleConfigurationChanged()

    def _reassignStepIndexedDevices(self) -> None:
        """Rebuild the step-indexed device buckets when step count changes."""
        with self.devicesLock:
            self._reassignStepIndexedDevicesNoLock()

    def _reassignStepIndexedDevicesNoLock(self) -> None:
        """Rebuild step-indexed buckets. Must be called with devicesLock held."""
        allStepDevices = [device for bucket in self.stepIndexedDevices for device in bucket]
        self.stepIndexedDevices = [[] for _ in range(self._cycleStepCount)]
        for index, device in enumerate(allStepDevices):
            targetStep = index % self._cycleStepCount
            self.stepIndexedDevices[targetStep].append(device)

    def _checkActivationTiming(
        self,
        activationTs: float,
        step: int,
        executionDurationS: float,
    ) -> None:
        """Validate step activation period against expected timing with 10% tolerance."""
        expectedPeriodS = self._cyclePeriodS
        if expectedPeriodS <= 0:
            self._lastActivationTs = activationTs
            return

        previousActivationTs = self._lastActivationTs
        self._lastActivationTs = activationTs
        if previousActivationTs is None:
            return

        actualPeriodS = activationTs - previousActivationTs
        allowedDeltaS = expectedPeriodS * self.ACTIVATION_TOLERANCE_RATIO
        if abs(actualPeriodS - expectedPeriodS) > allowedDeltaS:
            expectedPeriodMs = expectedPeriodS * 1000.0
            actualPeriodMs = actualPeriodS * 1000.0
            allowedDeltaMs = allowedDeltaS * 1000.0
            errorMs = abs(actualPeriodS - expectedPeriodS) * 1000.0
            print(
                f"[ERROR AstraComFetcher] Activation timing out of tolerance: "
                f"step={step} "
                f"expected={expectedPeriodMs:.3f}ms actual={actualPeriodMs:.3f}ms "
                f"runExec={executionDurationS * 1000.0:.3f}ms "
                f"error={errorMs:.3f}ms tolerance={allowedDeltaMs:.3f}ms"
            )

    def run(self) -> None:
        """Execute the periodic acquisition loop until stopped."""
        with self.actorsLock:
            actorsSnapshot = list(self.actors)
        with self.devicesLock:
            eachStepSnapshot = list(self.eachStepDevices)
            stepIndexedSnapshot = [list(bucket) for bucket in self.stepIndexedDevices]
        cycleStep = 0
        executionDurationS = 0.0
        self._cycleTimer.start()
        while self.running:
            if not self._cycleTimer.waitNextCycle():
                break

            if self._hasSharedUpdate:
                with self.actorsLock:
                    actorsSnapshot = list(self.actors)
                with self.devicesLock:
                    eachStepSnapshot = list(self.eachStepDevices)
                    stepIndexedSnapshot = [list(bucket) for bucket in self.stepIndexedDevices]
                self._hasSharedUpdate = False

            try:
                executionStartTs = time.monotonic()
                stepIntegrationDurationS = self._cyclePeriodS
                cycleIntegrationDurationS = self._cyclePeriodS * self._cycleStepCount

                self._checkActivationTiming(executionStartTs, cycleStep, executionDurationS)

                for actor in actorsSnapshot:
                    try:
                        actor.beforeMeasurements(cycleStep)
                    except Exception:
                        pass

                time.sleep(self.PHASE_DELAY_S)

                stepDevices = []
                if cycleStep < len(stepIndexedSnapshot):
                    stepDevices = stepIndexedSnapshot[cycleStep]

                for device in eachStepSnapshot:
                    try:
                        device.startMeasurement(cycleStep, stepIntegrationDurationS)
                    except Exception:
                        pass
                for device in stepDevices:
                    try:
                        device.startMeasurement(cycleStep, cycleIntegrationDurationS)
                    except Exception:
                        pass

                time.sleep(self.PHASE_DELAY_S)

                for device in eachStepSnapshot:
                    try:
                        device.getMeasurement(cycleStep, stepIntegrationDurationS)
                    except Exception:
                        pass
                for device in stepDevices:
                    try:
                        device.getMeasurement(cycleStep, cycleIntegrationDurationS)
                    except Exception:
                        pass

                executionDurationS = time.monotonic() - executionStartTs
                cycleStep = (cycleStep + 1) % self._cycleStepCount
            except Exception:
                pass
        self._cycleTimer.close()

    def stop(self) -> None:
        """Request thread termination and wait for completion."""
        self.running = False
        self._cycleTimer.requestStop()
        self.join()
        AstraComFetcher._instance = None
