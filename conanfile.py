from conan import ConanFile

class SpectatorCppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "spdlog/1.15.0",
        "gtest/1.14.0",
        "boost/1.83.0",
    )
    tool_requires = ()
    generators = "CMakeDeps", "CMakeToolchain"

    options = {
        # Static Boost — no runtime .so dependency for Python or Go consumers.
        "boost:shared": [True, False],
        # spdlog header-only — eliminates spdlog as a link dependency entirely.
        "spdlog:header_only": [True, False],
    }
    default_options = {
        "boost:shared": False,
        "spdlog:header_only": True,
    }
