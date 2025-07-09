# Spectator-CPP Libraries

This directory contains all the core libraries that make up the Spectator-CPP framework. These modular components work together to build the implementation of the main Spectator Registry interface.

## Library Overview

| Library  | Description                                                                                  |
|----------|----------------------------------------------------------------------------------------------|
| `config` | Configuration handling for the Spectator Registry metrics including output location and tags |
| `logger` | Logging facilities used throughout the framework                                             |
| `meter`  | Core metrics implementations including counters, gauges, timers, and meter identification    |
| `utils`  | Utility classes and helpers including singleton patterns                                     |
| `writer` | Output writers for metrics data (Memory, UDP, Unix Domain Socket)                      |

## Usage

Each library subfolder contains additional documentation describing its specific API and usage examples. See the main project README for complete integration instructions.

## Dependencies

Most libraries have minimal external dependencies, though some writers may require specific system capabilities for networking.