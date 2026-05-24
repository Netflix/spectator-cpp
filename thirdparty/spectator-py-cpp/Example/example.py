"""
Example usage of spectator-py-cpp.

Uses the memory writer so all metric lines can be inspected at the end
without requiring a running spectatord instance.
For production, replace WriterConfig("memory") with WriterConfig("udp").
"""

import time
from spectator import Registry, Config, WriterConfig

def main():
    # Use the memory writer to capture output for inspection.
    registry = Registry(Config(WriterConfig("memory")))

    # --- Counter ---
    # Monotonically increasing integer count, reported as a rate by Atlas.
    requests = registry.counter("server.requests", {
        "status": "200",
        "method": "GET",
        "endpoint": "/api/v1/data",
    })
    requests.increment()
    requests.add(5)

    # --- Gauge ---
    # Records the current value at a point in time.
    queue_depth = registry.gauge("queue.depth", {"queue": "ingest"})
    queue_depth.set(42.0)

    # Gauge with TTL — expires automatically if not updated within 30 seconds.
    active_sessions = registry.gauge("active.sessions", ttl_seconds=30)
    active_sessions.set(128.0)

    # --- MaxGauge ---
    # Tracks the maximum value seen since the last report interval.
    peak_memory = registry.max_gauge("memory.peak.bytes", {"region": "heap"})
    peak_memory.set(512 * 1024 * 1024)

    # --- AgeGauge ---
    # Tracks seconds since the last event — useful for staleness detection.
    last_sync = registry.age_gauge("data.last.sync.age", {"source": "primary"})
    last_sync.now()

    # --- MonotonicCounter ---
    # For values that only go up (e.g. OS counters). spectatord derives the rate.
    bytes_read = registry.monotonic_counter("bytes.read", {"device": "eth0"})
    bytes_read.set(1_000_000)

    bytes_written = registry.monotonic_counter_uint("bytes.written", {"device": "eth0"})
    bytes_written.set(500_000)

    # --- Timer ---
    # Records how long an operation took, in seconds.
    db_query = registry.timer("db.query.latency", {"table": "users"})
    start = time.monotonic()
    time.sleep(0.002)  # simulate 2ms of work
    db_query.record(time.monotonic() - start)

    # --- PercentileTimer ---
    # Like Timer but also publishes p50/p95/p99 percentile buckets.
    http_latency = registry.pct_timer("http.request.latency", {
        "method": "POST",
        "path": "/api/v1/ingest",
    })
    http_latency.record(0.042)  # 42ms

    # --- DistributionSummary ---
    # Tracks the distribution of sizes or amounts (e.g. payload bytes).
    payload_size = registry.dist_summary("http.request.size", {
        "endpoint": "/api/v1/ingest",
    })
    payload_size.record(4096)

    # --- PercentileDistributionSummary ---
    # Like DistributionSummary but also publishes percentile buckets.
    response_size = registry.pct_dist_summary("http.response.size", {
        "endpoint": "/api/v1/data",
    })
    response_size.record(1024)

    # --- Print all recorded lines from the memory writer ---
    lines = registry.dump_lines()
    print(f"Recorded {len(lines)} metric lines:")
    for line in lines:
        print(f"  {line}")


if __name__ == "__main__":
    main()
