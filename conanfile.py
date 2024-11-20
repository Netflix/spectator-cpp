from conan import ConanFile


class SpectatorCppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "abseil/20240722.0",
        "asio/1.32.0",
        "backward-cpp/1.6",
        "fmt/11.0.2",
        "gtest/1.15.0",
        "spdlog/1.15.0",
    )
    tool_requires = ()
    generators = "CMakeDeps", "CMakeToolchain"
