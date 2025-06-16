[![Build](https://github.com/Netflix/spectator-cpp/actions/workflows/build.yml/badge.svg)](https://github.com/Netflix/spectator-cpp/actions/workflows/build.yml)

# Spectator-cpp

This implements a basic [Spectator](https://github.com/Netflix/spectator) library for instrumenting Go applications. It
consists of a thin client designed to send metrics through [spectatord](https://github.com/Netflix-Skunkworks/spectatord).

## Instrumenting Code

```C++

```

## High-Volume Publishing

By default, the library sends every meter change to the spectatord sidecar immediately. This involves a blocking
`send` call and underlying system calls, and may not be the most efficient way to publish metrics in high-volume
use cases. For this purpose a simple buffering functionality in `Publisher` is implemented, and it can be turned
on by passing a buffer size to the `spectator::Config` constructor. It is important to note that, until this buffer
fills up, the `Publisher` will not send any meters to the sidecar. Therefore, if your application doesn't emit
meters at a high rate, you should either keep the buffer very small, or do not configure a buffer size at all,
which will fall back to the "publish immediately" mode of operation.

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