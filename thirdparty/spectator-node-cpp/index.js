'use strict';

const native = require('./build/Release/spectator_cpp.node');

// ---------------------------------------------------------------------------
// Config — mirrors nflx-spectator's Config class for drop-in compatibility
// ---------------------------------------------------------------------------

class Config {
  /**
   * @param {string} location - "udp", "udp://host:port", "unix", "unix:///path",
   *                            "memory", "noop". Defaults to "udp".
   * @param {object} extraCommonTags - tags added to every metric
   */
  constructor(location = 'udp', extraCommonTags = {}) {
    this.location = location;
    this.extraCommonTags = extraCommonTags || {};
    this.bufferSize = 0;
  }
}

// ---------------------------------------------------------------------------
// Meter classes — API compatible with nflx-spectator
// ---------------------------------------------------------------------------

class Counter {
  constructor(r, name, tags) {
    this._r = r;
    this._name = name;
    this._tags = tags || {};
  }

  /**
   * Increment the counter.
   * @param {number} [delta=1] - amount to add (must be > 0)
   */
  increment(delta) {
    if (delta !== undefined && delta !== 1) {
      this._r.counterAdd(this._name, BigInt(Math.trunc(delta)), this._tags);
    } else {
      this._r.counterIncrement(this._name, this._tags);
    }
  }

  add(delta) {
    this._r.counterAdd(this._name, BigInt(Math.trunc(delta)), this._tags);
  }
}

class Gauge {
  constructor(r, name, tags, ttlSeconds) {
    this._r = r;
    this._name = name;
    this._tags = tags || {};
    this._ttl = ttlSeconds;
  }

  set(value) {
    if (this._ttl != null) {
      this._r.gaugeSetTtl(this._name, value, this._ttl, this._tags);
    } else {
      this._r.gaugeSet(this._name, value, this._tags);
    }
  }
}

class MaxGauge {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  set(value) { this._r.maxGaugeSet(this._name, value, this._tags); }
}

class AgeGauge {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  set(seconds) { this._r.ageGaugeSet(this._name, seconds, this._tags); }
  now()        { this._r.ageGaugeNow(this._name, this._tags); }
}

class MonotonicCounter {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  set(amount) { this._r.monotonicCounterSet(this._name, amount, this._tags); }
}

class MonotonicCounterUint {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  set(amount) { this._r.monotonicCounterUintSet(this._name, BigInt(amount), this._tags); }
}

class Timer {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }

  /**
   * Record a duration.
   * @param {number|[number,number]} duration
   *   - number: nanoseconds (compatible with nflx-spectator)
   *   - [seconds, nanos]: hrtime tuple (compatible with process.hrtime())
   *   - Pass a value < 1e7 and it's treated as seconds (our native format).
   *   For maximum compatibility, pass hrtime tuples or nanoseconds.
   */
  record(duration) {
    let seconds;
    if (Array.isArray(duration)) {
      // hrtime tuple [seconds, nanoseconds]
      seconds = duration[0] + duration[1] / 1e9;
    } else if (duration > 1e7) {
      // large number → treat as nanoseconds (nflx-spectator convention)
      seconds = duration / 1e9;
    } else {
      // small number → treat as seconds (our convention)
      seconds = duration;
    }
    this._r.timerRecord(this._name, seconds, this._tags);
  }
}

class PercentileTimer {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(duration) {
    let seconds;
    if (Array.isArray(duration)) {
      seconds = duration[0] + duration[1] / 1e9;
    } else if (duration > 1e7) {
      seconds = duration / 1e9;
    } else {
      seconds = duration;
    }
    this._r.pctTimerRecord(this._name, seconds, this._tags);
  }
}

class DistributionSummary {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(amount) { this._r.distRecord(this._name, BigInt(amount), this._tags); }
}

class PercentileDistributionSummary {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(amount) { this._r.pctDistRecord(this._name, BigInt(amount), this._tags); }
}

// ---------------------------------------------------------------------------
// MemoryWriter — compatible with nflx-spectator's MemoryWriter
// ---------------------------------------------------------------------------

class MemoryWriter {
  constructor(nativeRegistry) {
    this._r = nativeRegistry;
  }

  /** @returns {string[]} all buffered metric lines */
  get() { return this._r.dumpLines(); }

  /** clears the buffer */
  clear() { this._r.clearWriter(); }
}

// ---------------------------------------------------------------------------
// Registry — compatible with nflx-spectator's Registry
// ---------------------------------------------------------------------------

const _defaultLogger = {
  debug: (...args) => {},   // silent by default
  info:  (...args) => console.info('[spectator]', ...args),
  error: (...args) => console.error('[spectator]', ...args),
  warn:  (...args) => console.warn('[spectator]', ...args),
};

class Registry {
  /**
   * @param {Config|string} configOrLocation
   *   - Config instance (nflx-spectator compatible)
   *   - string: location shorthand ("udp", "memory", etc.)
   * @param {number} [bufferSize=0]
   */
  constructor(configOrLocation = 'udp', bufferSize = 0) {
    let location, buf;
    if (configOrLocation instanceof Config) {
      location = configOrLocation.location;
      buf = configOrLocation.bufferSize || 0;
    } else {
      location = configOrLocation;
      buf = bufferSize;
    }
    this._r = new native.Registry(location, buf);
    this._location = location;
    this.logger = _defaultLogger;
  }

  // nflx-spectator API: r.writer() returns the writer object
  writer() {
    return new MemoryWriter(this._r);
  }

  counter(name, tags)                    { return new Counter(this._r, name, tags); }
  gauge(name, tags, ttlSeconds)          { return new Gauge(this._r, name, tags, ttlSeconds); }
  maxGauge(name, tags)                   { return new MaxGauge(this._r, name, tags); }
  ageGauge(name, tags)                   { return new AgeGauge(this._r, name, tags); }
  monotonicCounter(name, tags)           { return new MonotonicCounter(this._r, name, tags); }
  monotonicCounterUint(name, tags)       { return new MonotonicCounterUint(this._r, name, tags); }
  timer(name, tags)                      { return new Timer(this._r, name, tags); }
  pctTimer(name, tags)                   { return new PercentileTimer(this._r, name, tags); }
  distSummary(name, tags)                { return new DistributionSummary(this._r, name, tags); }
  pctDistSummary(name, tags)             { return new PercentileDistributionSummary(this._r, name, tags); }

  /** Returns all lines written to the memory writer as a string array. */
  dumpLines() { return this._r.dumpLines(); }
}

const { RuntimeMetrics } = require('./RuntimeMetrics');

module.exports = {
  Config,
  Registry,
  MemoryWriter,
  RuntimeMetrics,
  Counter, Gauge, MaxGauge, AgeGauge,
  MonotonicCounter, MonotonicCounterUint,
  Timer, PercentileTimer,
  DistributionSummary, PercentileDistributionSummary,
};
