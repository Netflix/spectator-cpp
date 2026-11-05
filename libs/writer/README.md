# Writer Module

The Writer module provides a flexible system for metric data output in the Spectator C++ library. It consists of 
several components that handle different aspects of data writing.

## Components

### Writer Types (`writer_types`)

This component defines the available writer types and their associated configurations:

- **Supported Writer Types:**
  - `Memory` - Writes data to an in-memory buffer (primarily for testing)
  - `UDP` - Writes data over UDP to a specified endpoint
  - `Unix` - Writes data to a Unix Domain Socket

- **Key Features:**
  - Type enumeration via `WriterType` enum class
  - String constants for type names in `WriterTypes` struct
  - Default locations for different writer types in `DefaultLocations` struct
  - Type-to-location mapping in `TypeToLocationMap`
  - String conversion via `WriterTypeToString()` function

### Writer Config (`writer_config`)

This component handles configuration of writers:

- **Key Features:**
  - Parses writer configuration from string identifiers
  - Handles environment variable overrides via `SPECTATOR_OUTPUT_LOCATION`
  - Error handling for invalid writer types
  - Provides a clean API for specifying writer type and location
  - Turns on buffering and sets its flush interval and full-buffer policy (see [Buffering](#buffering))

- **Usage Example:**
  ```cpp
  // Create a writer config with a specific type
  WriterConfig config(WriterTypes::UDP);
  
  // Or with a URL-style location
  const std::string unixUrl = std::string(WriterTypes::UnixURL) + "/var/run/custom/socket.sock";
  const WriterConfig config(unixUrl);
  
  // Environment variable overrides any provided value
  // SPECTATOR_OUTPUT_LOCATION=unix:///custom/path/socket.sock
  WriterConfig config(WriterTypes::UDP);  // Will use Unix socket instead

  // Buffering with 4096-byte buffers, the default flush interval and the default full-buffer policy
  WriterConfig config(WriterTypes::Unix, 4096);

  // Buffering with a flush interval of 250 ms and the drop policy
  WriterConfig config(WriterTypes::Unix, 4096, std::chrono::milliseconds(250), FullBufferPolicy::Drop);
  ```

### Writer Wrapper (`writer_wrapper`)

This component provides a wrapper around the different writer implementations:

- **Key Features:**
  - Common interface for all writer types
  - Factory pattern for creating writers based on configuration
  - Handles serialization and transmission of metric data
  - Abstracts transport details from the rest of the library
  - Buffers lines per thread and sends them from one sender thread when buffering is enabled

## Buffering

Without a buffer size, the thread that updates a meter sends the line itself, immediately. That mode is only safe
from one thread.

With a buffer size greater than 0 in the `WriterConfig`, the wrapper buffers per thread:

- Each thread that updates meters has its own buffer of at most the buffer size, created when the thread records
  its first line. Recording a line appends it to that buffer and touches nothing that other recording threads use.
- A thread hands its buffer over when the next line would not fit, or as soon as a line fills it exactly, and
  carries on with an empty buffer. It does not wait for the send.
- One sender thread takes the buffers that the threads have handed over and sends each as one datagram. A datagram
  holds whole lines and is never larger than the buffer size, except for a single line that is longer than the
  buffer size, which is sent as a datagram of its own.
- The lines of one thread are sent in the order in which the thread recorded them. There is no order between the
  lines of different threads.

| `WriterConfig` argument | Default                  | Meaning                                                             |
|-------------------------|--------------------------|---------------------------------------------------------------------|
| `bufferSize`            | `0` (no buffering)       | Size of each thread's buffer in bytes, and the largest datagram     |
| `flushInterval`         | `1000 ms`                | How often the sender collects and sends what the threads buffered   |
| `fullBufferPolicy`      | `FullBufferPolicy::Block`| What a thread does when it is out of buffer space                   |

### Flush interval

A thread that records rarely, or that records a burst and then goes idle, would otherwise hold its lines until it
fills a buffer or exits. Once per flush interval the sender thread therefore also takes the buffers that are still
being filled, after any full buffers of the same thread, and sends them. The default of 1 second keeps such lines
about a second behind at most, and costs at most one wake-up of the sender thread and one small datagram per
recording thread per second. The interval must be positive; the `WriterConfig` constructor throws a
`std::runtime_error` otherwise.

### Full-buffer policy

A thread may have up to 8 full buffers waiting for the sender thread, so a recording thread holds at most 9
buffers: those 8 and the one it is filling. The policy says what the thread does when it records a line that needs
a tenth:

- `FullBufferPolicy::Block` (default): the thread waits until the sender thread has taken one of its buffers. No
  line is lost. Only recording threads wait for the sender thread, never the other way around.
- `FullBufferPolicy::Drop`: the thread drops the line that it is recording and returns at once. Lines that are
  already buffered are kept. The sender thread logs the number of dropped lines as a warning, at most once per
  flush interval.
- `FullBufferPolicy::Send`: the thread sends its own oldest waiting buffer and then carries on. No line is lost,
  and the thread does not depend on the sender thread. This is the only case in which a recording thread sends,
  so the sender thread and the recording threads take turns on the socket. It does not send faster: there is one
  socket, and if the socket stalls, the thread waits in its send.

### Threads and process exit

- When a thread exits, what it has buffered goes to the sender thread, and the thread's buffers are released once
  they are sent. Handing them over never makes the exiting thread send or wait. The limit of 9 buffers therefore
  applies to each running thread: what exited threads have handed over waits for the sender thread without a
  limit, so many short-lived threads can build up a backlog while nothing can be sent.
- When the process exits normally, the writer stops the sender thread and then sends everything that is still
  buffered, including the lines of threads that are still running, before it closes the socket. This happens on
  the exiting thread, so the exit takes as long as those sends.
- Lines recorded after that shutdown has begun are dropped, and a thread that is waiting under the block policy is
  released.

### A receiver that is slow to read

The sender thread sends in bursts. Over UDP, datagrams that the receiver has no room for are lost without an error.
Over a Unix domain socket, Linux makes the sender wait until the receiver has read. macOS fails the send instead
("no buffer space"); in buffered mode the wrapper then retries for up to 250 ms before it gives the datagram up and
logs the failed send. Without buffering such a send fails at once.

## Integration

These components work together to provide a flexible metric output system:

1. `writer_types` defines the available writer types and their default configurations
2. `writer_config` handles parsing and validating configuration values
3. `writer_wrapper` instantiates the appropriate writer implementation

## Best Practices

- Use the environment variable `SPECTATOR_OUTPUT_LOCATION` for runtime configuration
- Prefer URL-style configurations for explicit endpoint specification
- For UDP: `udp://host:port`
- For Unix Domain Sockets: `unix:///path/to/socket`

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