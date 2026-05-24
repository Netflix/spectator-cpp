'use strict';

/**
 * RuntimeMetrics — port of nflx-spectator-nodejsmetrics to spectator-node-cpp.
 *
 * Collects Node.js runtime metrics (heap, GC, event loop, CPU, FDs) and
 * records them via a spectator Registry. Drop-in compatible with the old
 * nflx-spectator-nodejsmetrics API.
 *
 * Differences from the original:
 *  - GC metrics use PerformanceObserver instead of native C++ callbacks.
 *    Heap space stats before/after GC are not available (Node.js limitation).
 *  - FD metrics use /proc/self/fd on Linux; skipped on other platforms.
 *  - No native binding dependency (spectator_internals.node not needed).
 */

const v8 = require('v8');
const { performance, PerformanceObserver } = require('perf_hooks');

function toCamelCase(s) {
  return s.replace(/_([a-z])/g, (_, c) => c.toUpperCase());
}

function deltaMicros(end, start) {
  let deltaNanos = end[1] - start[1];
  let deltaSecs  = end[0] - start[0];
  if (deltaNanos < 0) { deltaNanos += 1e9; deltaSecs -= 1; }
  return Math.trunc(deltaSecs * 1e6 + deltaNanos / 1e3);
}

function updateV8HeapGauges(r, extraTags, heapInfo) {
  for (const key of Object.keys(heapInfo)) {
    r.gauge('nodejs.' + toCamelCase(key), extraTags).set(heapInfo[key]);
  }
}

function updateV8HeapSpaceGauges(r, extraTags, spaces) {
  for (const space of spaces) {
    const id = toCamelCase(space.space_name);
    for (const key of Object.keys(space)) {
      if (key !== 'space_name') {
        r.gauge('nodejs.' + toCamelCase(key), Object.assign({ id }, extraTags))
         .set(space[key]);
      }
    }
  }
}

/** Read current open / max FD counts on Linux via /proc. Returns null on other platforms. */
function getFdCounts() {
  if (process.platform !== 'linux') return null;
  const fs = require('fs');
  try {
    const used = fs.readdirSync('/proc/self/fd').length - 1; // subtract the readdir fd itself
    let max = null;
    try {
      const limits = fs.readFileSync('/proc/self/limits', 'utf8');
      const match = limits.match(/Max open files\s+(\d+)/);
      if (match) max = parseInt(match[1], 10);
    } catch (_) {}
    return { used, max };
  } catch (_) {
    return null;
  }
}

class RuntimeMetrics {
  constructor(r) {
    this.started = false;
    this._registry = r;
    this._intervals = [];
    this._gcObserver = null;

    const v = this.withVersion();
    this._external   = r.gauge('nodejs.external', v);
    this._heapTotal  = r.gauge('nodejs.heapTotal', v);
    this._heapUsed   = r.gauge('nodejs.heapUsed', v);
    this._rss        = r.gauge('nodejs.rss', v);

    this._eventLoopActive  = r.gauge('nodejs.eventLoopUtilization', v);
    this._eventLoopLagTimer = r.timer('nodejs.eventLoopLag', v);
    this._eventLoopTime    = r.timer('nodejs.eventLoop', v);

    this._allocationRate = r.counter('nodejs.gc.allocationRate', v);
    this._liveDataSize   = r.gauge('nodejs.gc.liveDataSize', v);
    this._maxDataSize    = r.gauge('nodejs.gc.maxDataSize', v);
    this._promotionRate  = r.counter('nodejs.gc.promotionRate', v);

    this._openFd = r.gauge('openFileDescriptorsCount', v);
    this._maxFd  = r.gauge('maxFileDescriptorsCount', v);

    this._cpuSystem = r.gauge('nodejs.cpuUsage', this.withVersion({ id: 'system' }));
    this._cpuUser   = r.gauge('nodejs.cpuUsage', this.withVersion({ id: 'user' }));

    this.lastEventLoop = undefined;
    this.lastEventLoopTime = undefined;
    this.eventLoopUtilization = undefined;
    this._lastNanos = 0;
    this._lastCpuUsage = undefined;
    this._lastCpuUsageTime = undefined;
    this._liveDataSizeCache = undefined;
  }

  withVersion(tags = {}) {
    tags['nodejs.version'] = process.version;
    return tags;
  }

  // --- GC metrics via PerformanceObserver ---

  _initGcMetrics() {
    const r = this._registry;
    this._gcObserver = new PerformanceObserver((list) => {
      for (const entry of list.getEntriesByType('gc')) {
        // entry.duration is in milliseconds → convert to seconds
        const seconds = entry.duration / 1000;
        const type = entry.detail && entry.detail.kind != null
          ? _gcKindName(entry.detail.kind) : 'unknown';
        r.timer('nodejs.gc.pause', this.withVersion({ id: type })).record(seconds);
      }
    });
    this._gcObserver.observe({ entryTypes: ['gc'] });
  }

  // --- Event loop lag ---

  static _measureEventLoopLag(self) {
    const now = process.hrtime();
    const nanos = now[0] * 1e9 + now[1];
    const lag = nanos - self._lastNanos - 1e9; // subtract 1s schedule period
    if (lag > 0) {
      self._eventLoopLagTimer.record([0, lag]); // hrtime tuple → seconds via Timer.record
    }
    self._lastNanos = nanos;
  }

  _initEventLoopLag() {
    const now = process.hrtime();
    this._lastNanos = now[0] * 1e9 + now[1];
    this._schedule(RuntimeMetrics._measureEventLoopLag, 1000, this);
  }

  // --- Event loop time ---

  static _measureEventLoopTime(self) {
    setImmediate(() => {
      const start = process.hrtime();
      setImmediate(() => {
        self._eventLoopTime.record(process.hrtime(start)); // hrtime tuple
      });
    });
  }

  _initEventLoopTime() {
    this._schedule(RuntimeMetrics._measureEventLoopTime, 500, this);
    RuntimeMetrics._measureEventLoopTime(this);
  }

  // --- Event loop utilization ---

  static _measureEventLoopUtilization(self) {
    if (!self.eventLoopUtilization) return;
    const now = process.hrtime();
    const nanos = now[0] * 1e9 + now[1];
    let deltaNanos;
    if (self.lastEventLoopTime) {
      const lastNanos = self.lastEventLoopTime[0] * 1e9 + self.lastEventLoopTime[1];
      deltaNanos = nanos - lastNanos;
    }
    const current = self.eventLoopUtilization();
    const active = current.active * 1e6;
    let deltaActive;
    if (self.lastEventLoop) {
      deltaActive = active - self.lastEventLoop.active * 1e6;
    }
    if (deltaActive && deltaNanos) {
      self._eventLoopActive.set(100.0 * deltaActive / deltaNanos);
    }
    self.lastEventLoopTime = now;
    self.lastEventLoop = current;
  }

  _initEventLoopUtilization() {
    if (typeof performance.eventLoopUtilization !== 'function') {
      this._registry.logger.info(
        `Unable to measure eventLoopUtilization. Requires Node.js >= 12.19.0: ${process.version}`
      );
      return;
    }
    this.eventLoopUtilization = performance.eventLoopUtilization.bind(performance);
    this.lastEventLoopTime = process.hrtime();
    this.lastEventLoop = this.eventLoopUtilization();
    this._schedule(RuntimeMetrics._measureEventLoopUtilization, 60000, this);
  }

  // --- CPU and heap ---

  static _measureCpuHeap(self) {
    const mem = process.memoryUsage();
    self._rss.set(mem.rss);
    self._heapTotal.set(mem.heapTotal);
    self._heapUsed.set(mem.heapUsed);
    self._external.set(mem.external);

    const newCpu = process.cpuUsage();
    const newTime = process.hrtime();
    if (self._lastCpuUsage && self._lastCpuUsageTime) {
      const elapsed = deltaMicros(newTime, self._lastCpuUsageTime);
      const userDelta   = newCpu.user   - self._lastCpuUsage.user;
      const systemDelta = newCpu.system - self._lastCpuUsage.system;
      self._cpuUser.set(userDelta   / elapsed * 100);
      self._cpuSystem.set(systemDelta / elapsed * 100);
    }
    self._lastCpuUsage     = newCpu;
    self._lastCpuUsageTime = newTime;

    updateV8HeapGauges(self._registry, self.withVersion(), v8.getHeapStatistics());
    updateV8HeapSpaceGauges(self._registry, self.withVersion(), v8.getHeapSpaceStatistics());
  }

  _initCpuHeap() {
    this._schedule(RuntimeMetrics._measureCpuHeap, 60000, this);
  }

  // --- File descriptors (Linux only) ---

  static _measureFd(self) {
    const fd = getFdCounts();
    if (!fd) return;
    self._openFd.set(fd.used);
    if (fd.max) self._maxFd.set(fd.max);
  }

  _initFd() {
    RuntimeMetrics._measureFd(this);
    this._schedule(RuntimeMetrics._measureFd, 60000, this);
  }

  // --- Lifecycle ---

  start() {
    if (this.started) {
      this._registry.logger.info('RuntimeMetrics already started');
      return;
    }
    this._initGcMetrics();
    this._initFd();
    this._initEventLoopLag();
    this._initEventLoopTime();
    this._initEventLoopUtilization();
    this._initCpuHeap();
    this.started = true;
  }

  stop() {
    for (const id of this._intervals) clearInterval(id);
    this._intervals = [];
    if (this._gcObserver) { this._gcObserver.disconnect(); this._gcObserver = null; }
    this.started = false;
  }

  _schedule(fn, ms, ...args) {
    const id = setInterval(fn, ms, ...args);
    id.unref();
    this._intervals.push(id);
  }
}

// Map PerformanceEntry GC kind numbers to type name strings
// https://nodejs.org/api/perf_hooks.html#performanceentrykind
function _gcKindName(kind) {
  switch (kind) {
    case 1:  return 'scavenge';
    case 2:  return 'minor';
    case 4:  return 'markSweepCompact';
    case 8:  return 'incremental';
    case 16: return 'weak';
    case 32: return 'node';
    default: return 'unknown';
  }
}

module.exports = { RuntimeMetrics };
