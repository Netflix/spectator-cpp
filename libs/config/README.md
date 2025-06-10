# Config Library

## Overview

The `Config` class serves as the configuration object for the Registry class, which manages metrics for the spectatorD system. It defines how and where metrics are written and what additional tags are applied to them.

## Usage

The `Config` class provides two constructors:

```cpp
// Constructor 1: Specify output location as string
Config(const std::string &location = "udp", 
       const std::unordered_map<std::string, std::string> &extra_tags = {});

// Constructor 2: Specify writer type directly (recommended)
Config(WriterType type,
       const std::unordered_map<std::string, std::string> &extra_tags = {});

// Examples:

// Example 1: Create a default config (UDP output)
auto defaultConfig = Config();

// Example 2: Config with memory storage and custom tags
auto memoryConfig = Config("memory", {
    {"app", "myService"},
    {"zone", "us-west-2"}
});

// Example 3: Config with direct writer type
auto stdoutConfig = Config(WriterTypes::Memory, {
    {"env", "production"}
});

// Example 4: Config writing to a specific file
auto fileConfig = Config("file:///var/log/metrics.log");

// Example 5: Config sending to specific UDP endpoint
auto customUdpConfig = Config("udp://metrics-collector.example.com:8125");
```

### When to Use Each Constructor

- Use the first constructor when you need to specify a custom output location.
- Use the second constructor (preferred) when working with standard writer types.

### Output Locations

Valid output locations include:
- `none`: Disable output
- `memory`: Store metrics in memory
- `stdout`: Write to standard output
- `stderr`: Write to standard error
- `udp`: Send metrics over UDP (default)
- `unix`: Use Unix domain sockets
- `file://path`: Write to a file
- `udp://host:port`: Send to specific UDP endpoint
- `unix://path`: Use specific Unix socket

### Environment Variables

- `SPECTATOR_OUTPUT_LOCATION`: Override the configured output location
- `TITUS_CONTAINER_NAME`: Set the `nf.container` tag automatically
- `TITUS_PROCESS_NAME`: Set the `nf.process` tag automatically

## Extra Tags

Extra tags are additional key-value pairs attached to all metrics. They can be specified during initialization and will be merged with environment-derived tags.