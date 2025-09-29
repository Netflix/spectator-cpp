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

    def configure(self):
        # Configure spdlog to be header-only
        self.options["spdlog"].header_only = True
        self.options["spdlog"].use_std_fmt = True

