from conan import ConanFile

class SpectatorCppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "spdlog/1.17.0",
        "gtest/1.17.0",
        "boost/1.90.0",
    )
    # Boost.Cobalt is disabled because b2 builds an extra boost_cobalt_io_ssl library
    # whenever it can find OpenSSL, but the boost/1.90.0 recipe does not list that library,
    # so package_info() aborts with "built, but were not used in any boost module". The
    # failure depends on whether OpenSSL headers happen to be visible on the build machine,
    # which is why it broke CI but not local Linux builds. We do not use Cobalt, and
    # disabling it stops b2 from building cobalt/cobalt_io/cobalt_io_ssl at all.
    default_options = {
        "boost/*:without_cobalt": True,
    }
    tool_requires = ()
    generators = "CMakeDeps", "CMakeToolchain"
