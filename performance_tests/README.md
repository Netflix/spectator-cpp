# Performance Tests

Measures the throughput of the spectator writer modes by repeatedly incrementing a counter from one or more producer threads over a fixed 2-minute window.

## Usage

```text
performance_test [writer_type] [write_mode] [num_threads] [buffer_size]
```

| Argument      | Values                                                    | Default  |
|---------------|-----------------------------------------------------------|----------|
| `writer_type` | `udp`, `uds`                                              | required |
| `write_mode`  | `0` NonBuffered, `1` Buffered, `2` ThreadLocalBuffered    | `0`      |
| `num_threads` | any positive integer                                      | `1`      |
| `buffer_size` | bytes, ignored for NonBuffered                            | `8192`   |

### Examples

```bash
# Single-threaded UDS with no buffering
./performance_test uds 0

# 4 threads using thread-local buffered mode over UDS
./performance_test uds 2 4

# 2 threads using buffered mode over UDP
./performance_test udp 1 2

# 4 threads with a 16KB buffer
./performance_test uds 2 4 16384
```

## Running the Full Test Suite

`run_perf_tests.sh` runs all writer/mode/thread combinations automatically and saves results to a timestamped directory.

```bash
./run_perf_tests.sh [binary] [udp|uds|both]
```

| Argument         | Default              | Description                       |
|------------------|----------------------|-----------------------------------|
| `binary`         | `./performance_test` | Path to the compiled test binary  |
| `udp\|uds\|both` | `both`               | Which writer transport(s) to test |

### Script Examples

```bash
# Run all combinations (both transports, all modes, 1/2/4/8 threads)
./run_perf_tests.sh

# Test only UDS transport
./run_perf_tests.sh ./performance_test uds

# Use a binary from a specific build directory
./run_perf_tests.sh ../cmake-build/performance_tests/performance_test udp
```

### What it runs

For each selected transport, the script runs two passes:

1. **Single-thread baseline** — all three modes (`NonBuffered`, `Buffered`, `ThreadLocalBuffered`) at 1 thread
2. **Multi-thread scaling** — `Buffered` and `ThreadLocalBuffered` at 2, 4, and 8 threads

### Results

Each run is saved to `results_YYYYMMDD_HHMMSS/<writer>_<mode>_<n>thread.txt` and also printed to stdout. After all runs complete the script prints the path to the results directory.

```text
results_20240315_143022/
├── uds_NonBuffered_1thread.txt
├── uds_Buffered_1thread.txt
├── uds_ThreadLocalBuffered_1thread.txt
├── uds_Buffered_2thread.txt
...
└── udp_ThreadLocalBuffered_8thread.txt
```

---

## Write Modes

| # | Mode | Description |
| --- | ---- | ----------- |
| `0` | NonBuffered | Every `Increment()` call is sent immediately with no buffering. |
| `1` | Buffered | Messages are batched into a shared buffer. A background thread flushes when full. |
| `2` | ThreadLocalBuffered | Each thread has its own buffer. No lock contention on the write path. A background flush thread sends stale data periodically. |

## Output

```text
Running performance test with the following configuration:
Writer Type: UDS
Write Mode: ThreadLocalBuffered
Threads: 4
Buffer Size: 4096 bytes

Performance Test Summary:
Threads used: 4
Write Mode: ThreadLocalBuffered
Iterations completed: 98123456
Total elapsed time: 120.00 seconds
Rate: 817695.47 iterations/second
Rate per thread: 204423.87 iterations/second/thread
```

- **Rate**: total `Increment()` calls per second across all threads
- **Rate per thread**: average per-thread throughput, useful for comparing scaling efficiency across modes
