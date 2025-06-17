# Config Library

## Overview

The `Config` class serves as the configuration object for the Registry class, which manages how metrics are sent to SpectatorD.
The `Config` constructor takes two parameters. The first parameter is required and is a `WriterConfig` object. This object defines
how metrics will be sent to `SpectatorD`. The second parameter `extraTags` is an unordered map of strings allowing you to provide additional tags to 
all of your metrics. Extra tags are additional key-value pairs attached to all metrics and will be merged with environment-derived tags.

## Usage

The `Config` class provides two constructors:

```cpp
// Constructor 1: Output location only

// Example 1
Config config(WriterConfig(WriterTypes::Memory));
Config config(WriterConfig(WriterTypes::UDP))

// Example 2
const std::string udpUrl = std::string(WriterTypes::UDPURL) + "192.168.1.100:8125";
Config config(WriterConfig(udpUrl));

// Constructor 2: Output location and tags

// Example 1
std::unordered_map<std::string, std::string> tags = {{"app", "test-app"}, {"env", "testing"}, {"region", "us-east-1"}};
Config config(WriterConfig(WriterTypes::Memory), tags);
```

## Environment Variables

If the following environment variables are set and not empty, there key and value will also be read and applied to your tags
- `TITUS_CONTAINER_NAME`: Set the `nf.container` tag automatically
- `TITUS_PROCESS_NAME`: Set the `nf.process` tag automatically

### Warning

If the environment variable `SPECTATOR_OUTPUT_LOCATION` is set this will override the value specified in the `WriterConfig`
read the `WriterConfig` readme.md for more details.