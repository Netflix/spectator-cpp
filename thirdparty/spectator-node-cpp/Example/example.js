'use strict';

/**
 * Example usage of spectator-node-cpp.
 *
 * Uses the memory writer so all metric lines can be inspected at the end
 * without requiring a running spectatord instance.
 * For production, replace 'memory' with 'udp' or 'unix'.
 */

const { Registry } = require('..');

function main() {
  const registry = new Registry('memory');

  // --- Counter ---
  // Monotonically increasing integer count, reported as a rate by Atlas.
  const requests = registry.counter('server.requests', {
    status:   '200',
    method:   'GET',
    endpoint: '/api/v1/data',
  });
  requests.increment();
  requests.add(5);

  // --- Gauge ---
  // Records the current value at a point in time.
  const queueDepth = registry.gauge('queue.depth', { queue: 'ingest' });
  queueDepth.set(42.0);

  // Gauge with TTL — expires automatically if not updated within 30 seconds.
  const activeSessions = registry.gauge('active.sessions', {}, 30);
  activeSessions.set(128.0);

  // --- MaxGauge ---
  const peakMemory = registry.maxGauge('memory.peak.bytes', { region: 'heap' });
  peakMemory.set(512 * 1024 * 1024);

  // --- AgeGauge ---
  const lastSync = registry.ageGauge('data.last.sync.age', { source: 'primary' });
  lastSync.now();

  // --- MonotonicCounter ---
  const bytesRead = registry.monotonicCounter('bytes.read', { device: 'eth0' });
  bytesRead.set(1_000_000);

  const bytesWritten = registry.monotonicCounterUint('bytes.written', { device: 'eth0' });
  bytesWritten.set(500_000);

  // --- Timer ---
  // Record a duration in seconds.
  const dbQuery = registry.timer('db.query.latency', { table: 'users' });
  const start = performance.now();
  // simulate 2ms of work
  for (let i = 0; i < 1e6; i++) {}
  dbQuery.record((performance.now() - start) / 1000);

  // --- PercentileTimer ---
  const httpLatency = registry.pctTimer('http.request.latency', {
    method: 'POST',
    path:   '/api/v1/ingest',
  });
  httpLatency.record(0.042); // 42ms

  // --- DistributionSummary ---
  const payloadSize = registry.distSummary('http.request.size', {
    endpoint: '/api/v1/ingest',
  });
  payloadSize.record(4096);

  // --- PercentileDistributionSummary ---
  const responseSize = registry.pctDistSummary('http.response.size', {
    endpoint: '/api/v1/data',
  });
  responseSize.record(1024);

  // --- Print all recorded lines from the memory writer ---
  const lines = registry.dumpLines();
  console.log(`Recorded ${lines.length} metric lines:`);
  for (const line of lines) {
    console.log(`  ${line}`);
  }
}

main();
