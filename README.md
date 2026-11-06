[![Build](https://github.com/Netflix/spectator-cpp/actions/workflows/build.yml/badge.svg)](https://github.com/Netflix/spectator-cpp/actions/workflows/build.yml)

# Spectator-cpp

This implements a basic [Spectator](https://github.com/Netflix/spectator) library for instrumenting CPP applications.
It consists of a thin client designed to send metrics through [spectatord](https://github.com/Netflix-Skunkworks/spectatord).

## High-Volume Publishing

By default, the library sends every meter change to the spectatord sidecar immediately. This involves a blocking
`send` call and underlying system calls on the thread that updates the meter, and it is only safe from one thread.
For high-volume use cases, and for recording from several threads, turn on buffering by passing a buffer size in
bytes to the `WriterConfig` constructor:

```cpp
// 4096-byte buffers, with the default flush interval (3 seconds) and full-buffer policy (block)
WriterConfig writerConfig(WriterTypes::Unix, 4096);

// Or: send what is buffered every 250 ms, and drop lines instead of waiting when a thread is out of buffer space
WriterConfig writerConfig(WriterTypes::Unix, 4096, std::chrono::milliseconds(250), FullBufferPolicy::Drop);

auto registry = Registry(Config(writerConfig));
```

With buffering, each thread that updates meters appends its lines to a buffer of its own, and one sender thread
sends the buffers to the sidecar. Updating a meter normally sends nothing, and threads do not wait for each other.

* **Datagrams.** A thread hands its buffer to the sender when the next line would not fit, or as soon as a line
  fills it exactly. Every datagram holds whole lines and is never larger than the buffer size, so choose a size that
  the transport can carry. The one exception is a single line that is longer than the buffer size: it is sent as a
  datagram of its own.
* **Order.** The lines of one thread are sent in the order in which that thread recorded them. Lines of different
  threads are sent in no particular order.
* **Flush interval.** The sender thread collects and sends what every thread has buffered once per flush interval,
  so a line never waits for its thread to fill a buffer. The default is 3 seconds: a line from a thread that records
  rarely, or that records a burst and then goes idle, reaches the sidecar about three seconds later at most, at the
  cost of at most one small datagram per recording thread every three seconds. Pass a different interval as the third argument of
  the `WriterConfig` constructor. It must be positive and at most 30 seconds.
* **Full-buffer policy.** A thread may have up to 8 full buffers waiting for the sender, so a recording thread
  holds at most 9 buffers: those 8 and the one it is filling. What a thread does when it needs more than that is the
  fourth argument of the `WriterConfig` constructor:
    * `FullBufferPolicy::Block` (the default): the thread waits until the sender has taken one of its buffers. No
      line is lost.
    * `FullBufferPolicy::Drop`: the thread drops the line that it is recording and returns at once, so it never
      waits. The number of dropped lines is logged as a warning, at most once per flush interval.
    * `FullBufferPolicy::Send`: the thread sends its own oldest waiting buffer and then carries on. No line is
      lost, and the thread does not depend on the sender thread. This is the only case in which a thread that
      updates a meter sends.
* **Exit.** A thread that exits hands what it has buffered to the sender. When the process exits normally, the
  writer shuts down: it stops the sender thread and sends everything that is still buffered, including the lines of
  threads that are still running. Lines recorded after the shutdown has begun are dropped.

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