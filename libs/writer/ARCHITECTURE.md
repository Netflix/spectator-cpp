# Writer Library: Architecture Reference

Internal reference for changing `libs/writer`. `README.md` is the short user-facing summary.

## 1. What it does

Each meter update becomes one newline-terminated protocol line, such as `c:my.counter:1.000000\n`. The writer delivers
those lines to spectatord over UDP, over a Unix datagram socket, into memory (tests), or nowhere (`none`). Delivery is
best effort. It never throws on the write path.

Delivery is **unbuffered** (one send per line) or **buffered** (each thread accumulates lines, and a background thread
sends them in batches).

## 2. Files

```text
writer_types/      transports and type definitions (no threading)
  include/base_writer.h      BaseWriter: Write(string_view), Close(), MaxPayloadSize()
  include/buffer_config.h    BufferConfig: size, flushInterval, dropOnFull
  include/writer_types.h     WriterType enum, names, default locations, lookup map
  include/{memory,noop,udp,uds}_writer.h, src/*.cpp, src/socket_options.h
  test_utils/                UDP and UDS receive servers, WaitFor, RecordingWriter
writer_config/     WriterConfig: type, location, BufferConfig
writer_wrapper/
  include/writer.h           Writer: the whole production wrapper (declaration)
  src/writer.cpp             Writer, ThreadBuffer, ThreadHandle, ExitGuard
  test/writer_test_helper.h  test-only friend of Writer
  test/test_writer.cpp       Writer through the singleton: UDS, UDP, death tests, modes
  test/test_writer_buffers.cpp   buffering, draining and shutdown on standalone writers
```

New `.cpp` files must also be added to the explicit source list in `spectator/CMakeLists.txt`.

## 3. Model

```text
Writer
 +-- transport             every send goes through one mutex
 +-- drain thread          sends the buffers on an interval
 +-- registry of buffers   one ThreadBuffer per writing thread

Each writing thread
 +-- a reference to its registered buffer, plus a guard that flushes it at thread exit
```

The four flows:

```text
Write        Writer::Write -> ThreadHandle: this thread's buffer -> Writer::Append
               unbuffered: Writer::WriteLine sends line + '\n' as one datagram
               fits                  append
               line > capacity       send pending, then the line alone
               full, dropOnFull      count the drop, wake the drain thread
               full                  send own buffer, then append
Drain        every flushInterval, or woken by a drop: visit each registered buffer -> send its pending bytes,
             then forget closed buffers
Thread exit  ~ThreadHandle -> RetireBuffer: flush and close the buffer
Shutdown     stop and join the drain thread -> retire every buffer -> close the transport
```

## 4. Pieces

- **Writer** is a singleton that is never destroyed, and is configured once, by one thread. It owns the
  transport, the settings (capacity, drop policy, flush interval), the roster, the drain thread and the shutdown
  sequence. `Writer::Write` is private; the meters are friends.
- **ThreadBuffer** is a plain struct: `pending`, `dataMutex`, `sendMutex` and an atomic `closed`. The Writer performs
  append, drain and retire on it. The registry, the owning thread and a drain snapshot share it through `shared_ptr`,
  because a drain pass can overlap the thread's exit.
- **ThreadHandle** (private to the Writer, `thread_local`) holds the thread's buffer reference and a scratch string for
  unbuffered sends. Its destructor retires the buffer and sets `t_handleGone`, a trivially destructible flag. A write
  that arrives after that, such as from a late `thread_local` destructor, goes through `SendDirect`.
- **ExitGuard** runs `ShutdownAtExit` at static destruction, before the Logger is destroyed.
- **Transport**: UDP and UDS report `min(65507, SO_SNDBUF)` as their largest datagram, and set `SO_SNDTIMEO` to 100 ms
  so a stalled receiver can't block a sender. The capacity is clamped to that limit, so a full buffer is one datagram.

## 5. Concurrency rules

- Lock order: shutdownMutex, passMutex, rosterMutex, a buffer's dataMutex, that buffer's sendMutex, transportMutex.
  drainMutex is never held while taking another.
- Whoever takes a buffer's pending content takes `sendMutex` **before** releasing `dataMutex`. That keeps each thread's
  lines in order. A drain pass releases `dataMutex` before sending, so appends never wait for I/O.
- The owning thread holds `dataMutex` across I/O only when it sends its own full buffer or an oversized line.
- `m_stop` is written only under the drain mutex. A drop wakes the drain thread through the same mutex.
- `m_configured` is stored with release ordering after the settings and transport are in place. A thread that loads it
  with acquire ordering can read them. Nothing else guards them, so `Initialize` must not run concurrently with itself.
- The transport is closed only after every buffer is closed.

## 6. Lifecycle

- **Initialize**: the first call wins. A later call logs a warning and is ignored, as is any call after the exit flush.
  `Registry` calls it. Each `Registry` still applies its own extra tags to its meters.
- **Thread exit**: the handle retires the buffer, which sends its lines. The drain thread forgets it on its next pass.
- **Process exit**: the `Writer` is never destroyed. `ExitGuard` runs `Shutdown`, and writes after that are dropped.
- **Fork** is not supported.

## 7. Guarantees

- `Write` is thread-safe in both modes.
- Datagrams hold only whole lines, and are never larger than the capacity unless one line is.
- A thread's lines arrive in order. Lines from different threads can arrive up to one flush interval out of order.
- Delivery is at most once. Staleness is bounded by the flush interval, a full buffer, or thread exit.
- Memory writers and `none` are always unbuffered.

## 8. Extending

- **New transport**: implement `BaseWriter` (and `MaxPayloadSize()` if it has a limit). Add the `WriterType` enumerator,
  name, `TypeToLocationMap` entry and `WriterTypeToString` case. Add a case to `MakeTransport` in `writer.cpp` and a
  branch in `Registry::Registry`. Decide in `EffectiveBufferConfig` whether it buffers. Add the `.cpp` to
  `spectator/CMakeLists.txt`.
- **New tunable**: add it to `BufferConfig`, then to the `WriterConfig` and `Config` getters, and read it in
  `Writer::Configure`.

## 9. Tests

CTest names: `writer_test`, `writer_buffers_test`, `writer_config_test`, `test_memory_writer`, `test_noop_writer`,
`test_udp_writer`, `test_uds_writer`.

- The writer is configured once per process, so tests reset it. `WriterTestHelper::Reset` destroys the process-wide
  writer and constructs a fresh one in the same storage, and the `Initialize` helpers call it first. `test_registry.cpp`
  does the same from a gtest listener. Reset lives in the test helper and one private `Writer` hook, not in any
  production path.
- `test_writer_buffers.cpp` builds standalone `Writer` objects over a recording transport. It drives the register,
  append and retire operations directly, so it doesn't depend on thread-exit handlers.
- Tests use `WaitFor` instead of fixed sleeps. Each binary has its own socket path or port, so `ctest -j` is safe.
