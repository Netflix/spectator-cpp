'use strict';

/**
 * Example usage of spectator-wasm.
 *
 * This example works in both Node.js and the browser.
 *
 * Browser note: metrics are buffered in Wasm memory and flushed via fetch.
 * In this example we just print the flushed lines — replace the flush handler
 * with a real fetch/WebSocket call in production.
 */

const { loadSpectator, Registry } = require('..');

async function main() {
  // Load the Wasm module — must be awaited before creating a Registry.
  const mod = await loadSpectator();
  const registry = new Registry(mod);

  // --- Counter ---
  const requests = registry.counter('server.requests', {
    status:   '200',
    method:   'GET',
    endpoint: '/api/v1/data',
  });
  requests.increment();
  requests.add(5);

  // --- Gauge ---
  const queueDepth = registry.gauge('queue.depth', { queue: 'ingest' });
  queueDepth.set(42.0);

  // Gauge with TTL — expires if not updated within 30 seconds.
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
  const dbQuery = registry.timer('db.query.latency', { table: 'users' });
  dbQuery.record(0.002); // 2ms

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

  // --- Flush buffered lines ---
  // In production, send these to a spectatord relay via fetch/WebSocket.
  // Here we just print them.
  const lines = registry.flush();
  const lineList = lines.split('\n').filter(l => l.length > 0);
  console.log(`Flushed ${lineList.length} metric lines:`);
  for (const line of lineList) {
    console.log(`  ${line}`);
  }

  // In a real app, set up periodic flushing:
  //
  // setInterval(() => {
  //   const batch = registry.flush();
  //   if (batch) {
  //     fetch('/metrics/relay', { method: 'POST', body: batch,
  //       headers: { 'Content-Type': 'text/plain' } });
  //   }
  // }, 5000);
}

main().catch(console.error);
