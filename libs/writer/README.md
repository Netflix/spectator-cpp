# Writer Module

The Writer module provides a flexible system for metric data output in the Spectator C++ library. It consists of several components that handle different aspects of data writing.

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
  ```

### Writer Wrapper (`writer_wrapper`)

This component provides a wrapper around the different writer implementations:

- **Key Features:**
  - Common interface for all writer types
  - Factory pattern for creating writers based on configuration
  - Handles serialization and transmission of metric data
  - Abstracts transport details from the rest of the library

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