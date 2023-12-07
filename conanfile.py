from conans import ConanFile


class SpectatorCppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "abseil/20230125.3",
        "asio/1.28.1",
        "backward-cpp/1.6",
        "fmt/10.1.1",
        "gtest/1.14.0",
        "spdlog/1.12.0"
    )
    generators = "cmake"
    default_options = {}
