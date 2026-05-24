'use strict';

/**
 * spectator-wasm — WebAssembly bindings for spectator-cpp.
 *
 * Works in browsers, Node.js, Deno, and edge runtimes (Cloudflare Workers, etc.).
 *
 * Because browsers cannot open raw UDP sockets, metrics are accumulated in
 * memory inside the Wasm module. Call registry.flush() periodically to retrieve
 * the buffered lines and relay them to spectatord via fetch/WebSocket.
 */

const SpectatorModule = require('./spectator.js');

/**
 * Load the Wasm module. Must be called before creating a Registry.
 * @returns {Promise<object>} The initialized Emscripten module.
 */
async function loadSpectator() {
  return await SpectatorModule();
}

class Counter {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  increment()     { this._r.counterIncrement(this._name, this._tags); }
  add(delta)      { this._r.counterAdd(this._name, Number(delta), this._tags); }
}

class Gauge {
  constructor(r, name, tags, ttlSeconds) {
    this._r = r; this._name = name; this._tags = tags || {}; this._ttl = ttlSeconds;
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
  set(amount) { this._r.monotonicCounterUintSet(this._name, Number(amount), this._tags); }
}

class Timer {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(seconds) { this._r.timerRecord(this._name, seconds, this._tags); }
}

class PercentileTimer {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(seconds) { this._r.pctTimerRecord(this._name, seconds, this._tags); }
}

class DistributionSummary {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(amount) { this._r.distRecord(this._name, Number(amount), this._tags); }
}

class PercentileDistributionSummary {
  constructor(r, name, tags) { this._r = r; this._name = name; this._tags = tags || {}; }
  record(amount) { this._r.pctDistRecord(this._name, Number(amount), this._tags); }
}

class Registry {
  /**
   * @param {object} module - The initialized Emscripten module from loadSpectator()
   */
  constructor(module) {
    this._r = new module.Registry();
  }

  counter(name, tags)              { return new Counter(this._r, name, tags); }
  gauge(name, tags, ttlSeconds)    { return new Gauge(this._r, name, tags, ttlSeconds); }
  maxGauge(name, tags)             { return new MaxGauge(this._r, name, tags); }
  ageGauge(name, tags)             { return new AgeGauge(this._r, name, tags); }
  monotonicCounter(name, tags)     { return new MonotonicCounter(this._r, name, tags); }
  monotonicCounterUint(name, tags) { return new MonotonicCounterUint(this._r, name, tags); }
  timer(name, tags)                { return new Timer(this._r, name, tags); }
  pctTimer(name, tags)             { return new PercentileTimer(this._r, name, tags); }
  distSummary(name, tags)          { return new DistributionSummary(this._r, name, tags); }
  pctDistSummary(name, tags)       { return new PercentileDistributionSummary(this._r, name, tags); }

  /**
   * Returns all buffered metric lines as a string (each ending with '\n').
   * Clears the internal buffer. Relay the result to spectatord via fetch/WebSocket.
   */
  flush() { return this._r.flush(); }
}

module.exports = {
  loadSpectator,
  Registry,
  Counter, Gauge, MaxGauge, AgeGauge,
  MonotonicCounter, MonotonicCounterUint,
  Timer, PercentileTimer,
  DistributionSummary, PercentileDistributionSummary,
};
