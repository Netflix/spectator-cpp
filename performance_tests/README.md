# Performance Tests

Measures the throughput of the spectator writer modes by repeatedly incrementing a counter from one or more producer threads over a fixed 2-minute window.

## Usage

```
performance_test [writer_type] [write_mode] [num_threads]
```

| Argument | Values | Default |
|----------|--------|---------|
| `writer_type` | `udp`, `uds` | required |
| `write_mode` | `0` NonBuffered, `1` Buffered, `2` LockFreeBuffered, `3` ThreadLocalBuffered | `0` |
| `num_threads` | any positive integer | `1` |

### Examples

```bash
# Single-threaded UDS with no buffering
./performance_test uds 0

# 4 threads using thread-local buffered mode over UDS
./performance_test uds 3 4

# 2 threads using lock-free buffered mode over UDP
./performance_test udp 2 2
```

## Write Modes

| Mode | Description |
|------|-------------|
| **NonBuffered** | Every `Increment()` call results in an immediate `sendto` syscall. Single-thread only. |
| **Buffered** | Messages are batched into a shared buffer (4096 bytes) protected by a mutex. A background thread flushes when the buffer is full. |
| **LockFreeBuffered** | Messages are pushed into a lock-free MPSC ring queue (8192 slots). A single consumer thread drains and batches them. Can drop messages under heavy multi-thread load if the consumer falls behind. |
| **ThreadLocalBuffered** | Each producer thread has its own buffer (4096 bytes). No shared queue or contention on the hot path. A background flush thread ensures stale buffers are flushed every 10 seconds. |

## Output

```
Running performance test with the following configuration:
Writer Type: UDS
Write Mode: ThreadLocalBuffered
Threads: 4

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
