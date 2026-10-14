# Config Library

## Overview

The `Config` class serves as the configuration object for the Registry class, which manages how metrics are sent to 
SpectatorD. The `Config` constructor takes three parameters. The first parameter is required and is a `WriterConfig`
object. This object defines how metrics will be sent to `SpectatorD`. The second parameter `extraTags` is an 
unordered map of strings allowing you to provide additional tags to all of your metrics. Extra tags are additional 
key-value pairs attached to all metrics and will be merged with environment-derived tags; on a key collision the
environment-derived value wins. The third parameter `includeEnvTags` defaults to `true` and controls whether the
environment-derived tags are added (see [Environment Variables](#environment-variables)).

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

If the following environment variables are set and not empty or whitespace-only, their values are added to your tags.
They take precedence over the same keys passed in `extraTags`.

- `NETFLIX_CONTAINER_NAME`, falling back to `TITUS_CONTAINER_NAME`: sets the `nf.container` tag.
- `NETFLIX_PROCESS_NAME`, falling back to the deprecated `TITUS_PROCESS_NAME`: sets the `nf.process` tag. Set it
  separately for each process, for example in the container's entrypoint: `NETFLIX_PROCESS_NAME=processA ./processA`.
  A container that runs several processes must give each one its own name. Use a stable, low-cardinality name made of
  the characters `-._A-Za-z0-9^~`; any other character is replaced with `_`.

To leave out both tags, pass `false` as the third parameter, `includeEnvTags`. Do this in a process that reports
metrics on behalf of other containers or processes, such as a relay or an agent like atlas-system-agent's pod monitor,
so that its own container and process names do not overwrite the tags of the metrics it reports. Tags passed in
`extraTags` are still added.

```cpp
Config config(WriterConfig(WriterTypes::UDP), {}, false);
```

### Warning

If the environment variable `SPECTATOR_OUTPUT_LOCATION` is set this will override the value specified in the 
`WriterConfig` read the `WriterConfig` readme.md for more details.