from conan import ConanFile

class SpectatorCppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "spdlog/1.17.0",
        "gtest/1.17.0",
        # Capped at 1.90.0: the boost/1.91.0 Conan recipe adds a cobalt_io_ssl component
        # that requires OpenSSL, but the recipe never requires it, so the library is never
        # built and package_info() fails. There is no without_cobalt_io_ssl option to
        # disable it.
        "boost/1.90.0",
    )
    tool_requires = ()
    generators = "CMakeDeps", "CMakeToolchain"
