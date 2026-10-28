[![Build](https://github.com/Netflix/spectator-cpp/actions/workflows/build.yml/badge.svg)](https://github.com/Netflix/spectator-cpp/actions/workflows/build.yml)

# Spectator-cpp

This implements a basic [Spectator](https://github.com/Netflix/spectator) library for instrumenting CPP applications.
It consists of a thin client designed to send metrics through [spectatord](https://github.com/Netflix-Skunkworks/spectatord).

## High-Volume Publishing

By default, the library sends every meter change to the spectatord sidecar immediately, with one `send` system call
per change. For high-volume use cases, turn on buffering by passing a buffer size to the `WriterConfig` constructor:

```cpp
// Buffer up to 60 KB per thread, sent at least every 5 seconds.
WriterConfig writerConfig(WriterTypes::Unix, 60000);

// Or set every option.
WriterConfig writerConfig(WriterTypes::Unix, BufferConfig{
    .size = 60000,
    .flushInterval = std::chrono::seconds(1),
    .dropOnFull = true,
});
```

With buffering on:

* Each thread that records meters appends to its own buffer, so threads don't contend with each other.
* A background thread sends every buffer at least once per flush interval, 5 seconds by default. The interval bounds
  how long a meter change can wait to be sent.
* When a thread's buffer is full, that thread sends it and carries on, so nothing is lost. With `dropOnFull`, the new
  line is dropped and counted instead, and the background thread is woken to empty the buffer. Use it when recording
  a meter must never make a system call.
* A thread's buffer is also sent when the thread exits, and every buffer is sent when the process exits normally.
* The buffer size is capped at the largest datagram the socket can send: 65507 bytes on Linux, less on macOS. Each
  buffer goes out as a single datagram.
* A thread's meter changes arrive in the order it made them. Changes from different threads can arrive up to one flush
  interval out of order, so a gauge set from several threads can briefly report an older value.
* Memory use is about one buffer for each thread that records meters.
* A send waits at most 100 ms for a sidecar that has stopped reading. A send that times out is logged and dropped.
* Only the `udp` and `unix` writers buffer. The `memory` writer, used in tests, and the `none` writer, which discards
  everything, send each change as it happens.

## Local & IDE Configuration

```shell
# setup python venv and activate, to gain access to conan cli
./setup-venv.sh
source venv/bin/activate

./build.sh  # [clean|clean --confirm|skiptest]
```

* Install the Conan plugin for CLion.
    * CLion > Settings > Plugins > Marketplace > Conan > Install
* Configure the Conan plugin.
    * The easiest way to configure CLion to work with Conan is to build the project first from the command line.
        * This will establish the `$PROJECT_HOME/CMakeUserPresets.json` file, which will allow you to choose the custom
          CMake configuration created by Conan when creating a new CMake project. Using this custom profile will ensure
          that sources are properly indexed and explorable.
    * Open the project. The wizard will show three CMake profiles.
        * Disable the default Cmake `Debug` profile.
        * Enable the CMake `conan-debug` profile.
    * CLion > View > Tool Windows > Conan > (gear) > Conan Executable: `$PROJECT_HOME/venv/bin/conan`