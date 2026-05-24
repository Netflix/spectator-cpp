"""
spectator-py-cpp — Python bindings for spectator-cpp.

The core metrics logic (ID construction, sanitization, wire formatting)
runs inside the C++ library via pybind11 for maximum performance.

Usage::

    from spectator import Registry, Config, WriterConfig

    registry = Registry(Config(WriterConfig("udp")))
    counter = registry.counter("requests", {"status": "200"})
    counter.increment()

    gauge = registry.gauge("queue.size")
    gauge.set(42.0)
"""

from spectator_cpp import Registry as _Registry, Config, WriterConfig

__all__ = ["Registry", "Config", "WriterConfig",
           "Counter", "Gauge", "MaxGauge", "AgeGauge",
           "MonotonicCounter", "MonotonicCounterUint",
           "Timer", "PercentileTimer",
           "DistributionSummary", "PercentileDistributionSummary"]


class Counter:
    """Monotonically increasing integer counter. Reports as a rate."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r = registry
        self._name = name
        self._tags = tags

    def increment(self) -> None:
        """Increment by 1."""
        self._r.counter_increment(self._name, self._tags)

    def add(self, delta: int) -> None:
        """Increment by delta (must be > 0)."""
        self._r.counter_add(self._name, int(delta), self._tags)


class Gauge:
    """Records the current value at a point in time."""
    __slots__ = ("_r", "_name", "_tags", "_ttl")

    def __init__(self, registry: "_Registry", name: str, tags: dict, ttl_seconds: int = None):
        self._r = registry
        self._name = name
        self._tags = tags
        self._ttl = ttl_seconds

    def set(self, value: float) -> None:
        if self._ttl is not None:
            self._r.gauge_set_ttl(self._name, float(value), self._ttl, self._tags)
        else:
            self._r.gauge_set(self._name, float(value), self._tags)


class MaxGauge:
    """Tracks the maximum value seen."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def set(self, value: float) -> None:
        self._r.max_gauge_set(self._name, float(value), self._tags)


class AgeGauge:
    """Tracks time since the last event, in seconds."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def set(self, seconds: float) -> None:
        self._r.age_gauge_set(self._name, float(seconds), self._tags)

    def now(self) -> None:
        """Record the current time as the most recent event."""
        self._r.age_gauge_now(self._name, self._tags)


class MonotonicCounter:
    """Monotonically increasing double. spectatord derives a rate."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def set(self, amount: float) -> None:
        self._r.monotonic_counter_set(self._name, float(amount), self._tags)


class MonotonicCounterUint:
    """Monotonically increasing uint64."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def set(self, amount: int) -> None:
        self._r.monotonic_counter_uint_set(self._name, int(amount), self._tags)


class Timer:
    """Records durations in seconds."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def record(self, seconds: float) -> None:
        self._r.timer_record(self._name, float(seconds), self._tags)


class PercentileTimer:
    """Records durations in seconds with percentile bucketing."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def record(self, seconds: float) -> None:
        self._r.pct_timer_record(self._name, float(seconds), self._tags)


class DistributionSummary:
    """Records sizes or amounts as a distribution."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def record(self, amount: int) -> None:
        self._r.dist_record(self._name, int(amount), self._tags)


class PercentileDistributionSummary:
    """Records sizes or amounts with percentile bucketing."""
    __slots__ = ("_r", "_name", "_tags")

    def __init__(self, registry: "_Registry", name: str, tags: dict):
        self._r, self._name, self._tags = registry, name, tags

    def record(self, amount: int) -> None:
        self._r.pct_dist_record(self._name, int(amount), self._tags)


class Registry:
    """
    Main entry point for recording metrics.

    Example::

        registry = Registry(Config(WriterConfig("udp")))
        registry.counter("requests", {"status": "200"}).increment()
        registry.gauge("memory.used").set(1024 * 1024 * 512)
    """

    def __init__(self, config: Config):
        self._r = _Registry(config)

    def counter(self, name: str, tags: dict = None) -> Counter:
        return Counter(self._r, name, tags or {})

    def gauge(self, name: str, tags: dict = None, ttl_seconds: int = None) -> Gauge:
        return Gauge(self._r, name, tags or {}, ttl_seconds)

    def max_gauge(self, name: str, tags: dict = None) -> MaxGauge:
        return MaxGauge(self._r, name, tags or {})

    def age_gauge(self, name: str, tags: dict = None) -> AgeGauge:
        return AgeGauge(self._r, name, tags or {})

    def monotonic_counter(self, name: str, tags: dict = None) -> MonotonicCounter:
        return MonotonicCounter(self._r, name, tags or {})

    def monotonic_counter_uint(self, name: str, tags: dict = None) -> MonotonicCounterUint:
        return MonotonicCounterUint(self._r, name, tags or {})

    def timer(self, name: str, tags: dict = None) -> Timer:
        return Timer(self._r, name, tags or {})

    def pct_timer(self, name: str, tags: dict = None) -> PercentileTimer:
        return PercentileTimer(self._r, name, tags or {})

    def dist_summary(self, name: str, tags: dict = None) -> DistributionSummary:
        return DistributionSummary(self._r, name, tags or {})

    def pct_dist_summary(self, name: str, tags: dict = None) -> PercentileDistributionSummary:
        return PercentileDistributionSummary(self._r, name, tags or {})

    def dump_lines(self) -> list:
        """Return all lines written to the memory writer as a list of strings.
        Only meaningful when the registry was created with WriterConfig('memory')."""
        return self._r.dump_lines()
