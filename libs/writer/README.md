# Writer

The writer subsystem is responsible for sending metric messages to a destination (UDP socket, Unix domain socket, or in-memory for testing). It is organized into four layers, each with a single responsibility.

```
writer_wrapper   (public API — Writer singleton)
      │
writer_modes     (buffering strategy — owns the writer)
      │
writer_types     (transport — sends bytes to a destination)
      │
writer_config    (configuration parsing)
```

---

## Layers

### `writer_types` — Transport

Defines how bytes are physically sent.

**`BaseWriter`** is the abstract base class with three pure virtual methods:

| Method | Description |
|--------|-------------|
| `Send(message)` | Transmit a newline-terminated string |
| `Close()` | Release the underlying socket/resource |
| `GetType()` | Return the `WriterType` enum value |

Three concrete implementations are provided:

| Class | `WriterType` | Description |
|-------|-------------|-------------|
| `MemoryWriter` | `Memory` | Stores messages in-memory; used for testing |
| `UDPWriter` | `UDP` | Sends over a UDP socket (Boost.Asio) |
| `UDSWriter` | `Unix` | Sends over a Unix domain socket (Boost.Asio) |

---

### `writer_modes` — Buffering Strategy

Defines _when_ messages are sent. Each mode owns the `BaseWriter` and is responsible for creating it via `WriteMode::CreateWriter()`.

**`WriteMode`** is the abstract base class. Its protected constructor accepts `(WriterType, param, port)` and calls `CreateWriter()` internally, so derived classes never interact with raw `BaseWriter` construction directly.

| Class | `WriteModeType` | Description |
|-------|----------------|-------------|
| `NonBufferedWriteMode` | `NonBuffered` | Calls `Send()` immediately on every `Write()` |
| `BufferedWriteMode` | `Buffered` | Accumulates messages in a string buffer; a background thread flushes when the buffer is full. Remaining data is flushed on destruction. |
| `ThreadLocalBufferedWriteMode` | `ThreadLocalBuffered` | Each thread has its own independent buffer. A background flush thread periodically sends stale data (default interval: 10 s). Buffers are also flushed when a thread exits or the buffer reaches the size threshold. |

#### BufferedWriteMode — shutdown sequence

1. Caller destroys the `BufferedWriteMode` object.
2. Destructor sets `m_shutdown = true` and notifies all waiting threads.
3. The background `ThreadSend` thread wakes, swaps out any remaining buffer content, sends it, then exits.

#### ThreadLocalBufferedWriteMode — shutdown sequence

1. Caller destroys the `ThreadLocalBufferedWriteMode` object.
2. Destructor sets `m_shutdown = true` and joins the flush thread.
3. Destructor then iterates over all registered `ThreadBuffer`s and flushes any remaining data.
4. When a worker thread exits, its `ThreadLocalData` destructor flushes and deregisters its buffer (if shutdown has not already started).

---

### `writer_config` — Configuration

`WriterConfig` parses a type string into a `WriterType` + destination location and validates buffering parameters.

**Accepted type strings:**

| String | Transport | Default location |
|--------|-----------|-----------------|
| `"memory"` | `MemoryWriter` | _(none)_ |
| `"udp"` | `UDPWriter` | `udp://127.0.0.1:1234` |
| `"unix"` | `UDSWriter` | `unix:///run/spectatord/spectatord.unix` |
| `"udp://host:port"` | `UDPWriter` | as specified |
| `"unix:///path"` | `UDSWriter` | as specified |

The environment variable `SPECTATOR_OUTPUT_LOCATION` overrides the type string when set.

**Validation rules:**
- `bufferSize` must be `> 0` when using `Buffered` or `ThreadLocalBuffered` mode.
- `bufferSize` must be `0` when using `NonBuffered` mode.

---

### `writer_wrapper` — Public API

`Writer` is a singleton (via `Singleton<Writer>`) that exposes two static methods to the rest of the codebase:

```cpp
Writer::Initialize(type, param, port, bufferSize, modeType);
Writer::Write(message);
```

`Initialize` constructs the appropriate `WriteMode` (which constructs the appropriate `BaseWriter` internally). `Write` delegates to the active `WriteMode`.

`WriterTestHelper` is a friend class that grants tests access to `Initialize` and the underlying `BaseWriter*` without exposing them publicly.
