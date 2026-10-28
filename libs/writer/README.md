# Writer Module

The Writer module delivers protocol lines from meters to spectatord. It has three components: the transports in
`writer_types`, the configuration in `writer_config`, and the process-wide `Writer` in `writer_wrapper`, which owns
the transport plus the buffering and draining machinery.

## Components

### Writer Types (`writer_types`)

Transports and the types that describe them.

- **Supported Writer Types:**
  - `Memory` (`"memory"`): keeps every payload in memory; used by tests.
  - `UDP` (`"udp"`): sends datagrams over UDP.
  - `Unix` (`"unix"`): sends datagrams over a Unix domain socket.
  - `Noop` (`"none"`): discards everything, to turn metrics off.

- **Key Features:**
  - Type enumeration via the `WriterType` enum class.
  - String constants for type names in `WriterTypes`, and default locations in `DefaultLocations`.
  - Type-to-location mapping in `TypeToLocationMap`, and `WriterTypeToString()`.
  - `BufferConfig`: buffer size, flush interval and drop-on-full (see below).

- **Transports** implement `BaseWriter`:
  - `Write(std::string_view)` delivers one payload as one datagram.
  - `Close()` closes the transport.
  - `MaxPayloadSize()` optionally reports the largest payload one `Write` can deliver. It defaults to unlimited.
  - Transports don't need to be thread-safe, because the Writer serializes every call.
  - The UDP and Unix writers read their limit from the socket's send buffer, capped at 65507 bytes. That is 65507
    on Linux, and 9216 for UDP and 2048 for Unix sockets on macOS.
  - Their sends time out after 100 ms, so a stalled spectatord can't block the application.

### Writer Config (`writer_config`)

This component handles configuration of writers:

- **Key Features:**
  - Parses writer configuration from string identifiers
  - Handles environment variable overrides via `SPECTATOR_OUTPUT_LOCATION`
  - Error handling for invalid writer types and flush intervals
  - Buffering options, through a buffer size or a full `BufferConfig`

- **Usage Example:**
  ```cpp
  // Create a writer config with a specific type
  WriterConfig config(WriterTypes::UDP);

  // Or with a URL-style location
  const std::string unixUrl = std::string(WriterTypes::UnixURL) + "/var/run/custom/socket.sock";
  const WriterConfig config(unixUrl);

  // Buffer up to 60 KB per thread with the default flush interval (5 s) and backpressure
  WriterConfig config(WriterTypes::Unix, 60000);

  // Or choose every buffering option
  WriterConfig config(WriterTypes::Unix, BufferConfig{
      .size = 60000,
      .flushInterval = std::chrono::seconds(1),
      .dropOnFull = true,
  });

  // Environment variable overrides any provided value
  // SPECTATOR_OUTPUT_LOCATION=unix:///custom/path/socket.sock
  WriterConfig config(WriterTypes::UDP);  // Will use Unix socket instead
  ```

### Writer Wrapper (`writer_wrapper`)

The process-wide `Writer` singleton that every meter writes through. It is built from three parts:

- **`Sink`** wraps one transport, a single socket. A mutex serializes every send, so the writing threads and the
  drain thread share one socket.
- **`ThreadBuffer`** is one thread's accumulation buffer of newline-terminated lines. It never holds more than the
  transport's largest payload, so a full buffer is always sent with a single write.
- **`ThreadRegistry`** is the roster of every thread's buffer, plus the drain thread that empties them. It runs a pass
  every flush interval, or sooner after a drop. Each pass sends each buffer's lines, then forgets the buffers of
  threads that have exited.

The buffer size selects one of two modes:

- **Unbuffered** (size 0, and always for `memory` and `none`): every line goes straight to the sink.
- **Buffered** (size above 0, for `udp` and `unix`):
  - A thread registers a buffer on its first write. After that, appending takes one uncontended lock.
  - When a buffer is full, the default is that the writing thread sends it and carries on, so nothing is lost.
  - With `dropOnFull`, the line is dropped and counted instead, and the drain thread is woken to empty the buffer. The
    writing thread never makes a system call.
  - A line longer than the whole buffer is always sent on its own; it is never dropped or truncated.

Behavior:

- **Ordering:** a thread's lines arrive in the order it wrote them. Lines from different threads can arrive up to one
  flush interval out of order.
- **Thread exit:** a thread's buffer is sent when the thread exits, and the drain thread forgets it on its next pass.
- **Reconfiguration:** each `Initialize` stops and joins the old drain thread, sends every buffer, and closes the old
  sink, then installs the new configuration. Threads move to the new configuration on their next write.
- **Process exit:** pending lines are sent when the process exits normally. The `Writer` itself is never destroyed, so
  a thread still writing during exit can't touch a destroyed object; its later lines are dropped.
- **Fork:** forking after the writer has started its drain thread is not supported.

A new transport, such as shared memory, only needs to implement `BaseWriter`. It can report `MaxPayloadSize()` if it
has a limit. The buffering, draining and backpressure code doesn't change.

## Integration

These components work together to provide a flexible metric output system:

1. `writer_types` defines the available writer types, their default locations and the transports
2. `writer_config` handles parsing and validating configuration values
3. `writer_wrapper` instantiates the transport and buffers, drains and sends every meter's lines

## Best Practices

- Use the environment variable `SPECTATOR_OUTPUT_LOCATION` for runtime configuration
- Prefer URL-style configurations for explicit endpoint specification
- For UDP: `udp://host:port`
- For Unix Domain Sockets: `unix:///path/to/socket`
- Turn on buffering for high-volume publishing. Keep the default backpressure unless recording a meter must never block
  on I/O.

## Example

```cpp
// Create a configuration
const std::string udpUrl = std::string(WriterTypes::UDPURL) + "192.168.1.100:8125";
const WriterConfig writerConfig(udpUrl);

// Create a configuration object with the writer config
Config config(writerConfig);

// Additional tags can be added
std::unordered_map<std::string, std::string> tags = {
    {"app", "my-application"},
    {"env", "production"}
};
Config config(writerConfig, tags);

// The config can be used to create a registry
auto registry = spectator::Registry(config);
```
