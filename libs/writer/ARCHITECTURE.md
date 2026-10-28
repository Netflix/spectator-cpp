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
  include/writer.h           Writer facade declaration
  include/sink.h             Sink: mutex-guarded transport
  include/thread_buffer.h    ThreadBuffer: one thread's accumulation buffer
  include/thread_registry.h  ThreadRegistry: roster of buffers, drain thread, reaping
  include/writer_test_helper.h  test-only friend of Writer
  src/                       writer.cpp (facade, ThreadHandle, exit flush), sink.cpp, thread_buffer.cpp,
                             thread_registry.cpp
  test/                      test_writer.cpp, test_thread_buffer.cpp, test_thread_registry.cpp
```

New `.cpp` files must also be added to the explicit source list in `spectator/CMakeLists.txt`.

## 3. Flow

```text
Counter::Increment() -> Writer::Write(line)
  unbuffered: Sink::Send(line + '\n')
  buffered:   ThreadHandle (thread_local) -> ThreadBuffer::Append(line)
                fits                  append
                full, dropOnFull      count the drop, wake the drain thread
                full, !dropOnFull     send own buffer through the Sink, then append
                line > capacity       send pending, then the line alone

ThreadRegistry drain thread, every flushInterval or when woken by a drop:
  each buffer: copy and clear pending, Sink::Send; then forget closed buffers

Thread exit: ~ThreadHandle -> ThreadBuffer::Close() (sends the rest)
```

## 4. Components

- **Sink**: one transport and a mutex. Every send goes through it, so threads share one socket. `Send` returns false
  once closed. No chunking: the buffer capacity is clamped to `BaseWriter::MaxPayloadSize()`, so a full buffer is one
  datagram. UDP and UDS report `min(65507, SO_SNDBUF)`; they set `SO_SNDTIMEO` to 100 ms so a stalled receiver can't
  block a sender.
- **ThreadBuffer**: `pending` string, `dataMutex`, `sendMutex`, atomic `closed`. The owning thread calls `Append`.
  Others call `Drain` and `Close`. An optional `onDrop` callback is how the registry learns of drops.
- **ThreadRegistry**: roster (`shared_ptr<ThreadBuffer>`), the drain `std::thread` (declared last), drop counter.
  `Register`, `DrainOnce`, `Close`, `Size`, `Dropped`.
- **Writer**: leaky singleton. Holds `m_sink`, `m_registry` (null when unbuffered), and `m_mutex`, which serializes
  `Initialize`, `Shutdown` and attaching. Threads attach to the current configuration on their first write.

## 5. Concurrency rules

- Lock order: `Writer::m_mutex`, then registry `Close` (joins the drain thread, takes the pass mutex), then roster
  mutex, then buffer `dataMutex`, then `sendMutex`, then the Sink mutex. The drain thread never takes `m_mutex`.
- Whoever takes a buffer's pending content takes `sendMutex` **before** releasing `dataMutex`. That keeps each thread's
  lines in order. `Drain` releases `dataMutex` before sending, so appends never wait for I/O.
- The owning thread holds `dataMutex` across I/O only when it sends its own full buffer or an oversized line.
- `stop` is written only under the drain mutex. A drop wakes the drain thread through the same mutex.
- The Sink is closed only after every buffer that uses it is closed.

## 6. Lifecycle

- **Initialize**: builds the transport, Sink and registry first, so a throw leaves the old setup running. Then, under
  `m_mutex`: close the old registry (join, flush all buffers), close the old Sink, install the new pair. Threads find
  their old buffer or sink closed on the next write and attach to the new one.
- **Thread exit**: the handle's destructor sends the thread's lines and closes the buffer. The drain thread forgets it
  on its next pass. A `thread_local` destructor that writes afterwards sends its line directly (`t_handleDestroyed`).
- **Process exit**: the `Writer` is never destroyed. A function-local `ExitFlush` runs `Shutdown` at static
  destruction, before the Logger is destroyed, and refuses later `Initialize` calls.
- **Fork** is not supported.

## 7. Guarantees

- `Write` is thread-safe in both modes.
- Datagrams hold only whole lines, and are never larger than the capacity unless one line is.
- A thread's lines arrive in order. Lines from different threads can arrive up to one flush interval out of order.
- Delivery is at most once. Staleness is bounded by the flush interval, a full buffer, or thread exit.
- Memory writers and `none` are always unbuffered.

## 8. Extending

- **New transport**: implement `BaseWriter` (and `MaxPayloadSize()` if it has a limit). Add the `WriterType`
  enumerator, name, `TypeToLocationMap` entry and `WriterTypeToString` case. Add a case to `Writer::Initialize` and a
  branch in `Registry::Registry`. Decide in `Writer::Initialize` whether it buffers. Add the `.cpp` to
  `spectator/CMakeLists.txt`.
- **New tunable**: add it to `BufferConfig`, then to `WriterConfig` and `Config` getters, and use it in
  `Writer::Install` or `RegistryOptions`.

## 9. Tests

CTest names: `writer_test`, `thread_buffer_test`, `thread_registry_test`, `writer_config_test`, `test_memory_writer`,
`test_noop_writer`, `test_udp_writer`, `test_uds_writer`. Tests use `WaitFor` instead of fixed sleeps. Each binary has
its own socket path or port, so `ctest -j` is safe, except `test_udp_writer` and `writer_test`'s UDP case, which use
different ports on purpose.
