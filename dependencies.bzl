# Due to https://github.com/bazelbuild/bazel/issues/2757
# Consumers have to specify all of the transitive dependencies recursively which is not a very nice UX and pretty error
# prone, this defines a function which declares all the spectator dependencies so that any Bazel project needing to
# depend on spectator can just load and call this function instead of re-declaring all dependencies.

# There's no clear consensus for reusing transitive dependencies, most of the community names them in reverse fqdn
# (though sometimes this may include the org name, sometimes just the repo name) and create a set of parameters
# omit_xxx where xxx is the reverse fqdn of the dependency so that if the consumer happens to already have it, they can
# omit_xxx = True when calling the dependency declaration function and reuse their version. See
# https://github.com/google/nomulus/blob/master/java/google/registry/repositories.bzl and the corresponding
# https://github.com/google/nomulus/blob/master/WORKSPACE. Discussion https://github.com/bazelbuild/bazel/issues/1952.
#
# New cmake_external style dependencies are referenced in a different way (e.g. "//external:<name>") so in order to
# provide a way for the consumer to use their own version of the dependency which is pulled in a different way
# i.e. cmake_external vs Bazel native we provide configuration settings and select the name based on those.
# Probably easier to eventually let consumers specify the names altogether.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def spectator_dependencies(
        omit_curl = False,
        omit_com_github_c_ares_c_ares = False,
        omit_boringssl = False,
        omit_net_zlib = False,
        omit_com_github_skarupke_flat_hash_map = False,
        omit_com_github_tessil_hopscotch_map = False,
        omit_com_github_fmtlib_fmt = False,
        omit_com_github_tencent_rapidjson = False,
        omit_com_github_gabime_spdlog = False,
        omit_com_google_googletest = False,
        omit_com_google_googlebench = False):
    if not omit_curl:
        _curl()

    # Optional curl dependency used to fix https://stackoverflow.com/questions/9191668/error-longjmp-causes-uninitialized-stack-frame.
    if not omit_com_github_c_ares_c_ares:
        _com_github_c_ares_c_ares()

    # curl dependency.
    if not omit_boringssl:
        _boringssl()

    # curl dependency.
    if not omit_net_zlib:
        _net_zlib()

    if not omit_com_github_skarupke_flat_hash_map:
        _com_github_skarupke_flat_hash_map()

    if not omit_com_github_tessil_hopscotch_map:
      _com_github_tessil_hopscotch_map()

    if not omit_com_github_fmtlib_fmt:
        _com_github_fmtlib_fmt()

    if not omit_com_github_tencent_rapidjson:
        _com_github_tencent_rapidjson()

    if not omit_com_github_gabime_spdlog:
        _com_github_gabime_spdlog()

    if not omit_com_google_googletest:
        _com_google_googletest()

    if not omit_com_google_googlebench:
        _com_google_googlebench()

def _curl():
    # https://github.com/tensorflow/tensorflow/blob/master/tensorflow/workspace.bzl.
    http_archive(
        # Needs build file updates to build with reverse fqdn.
        name = "curl",
        build_file = "@spectator//third_party:curl.BUILD",
        sha256 = "4376ac72b95572fb6c4fbffefb97c7ea0dd083e1974c0e44cd7e49396f454839",
        strip_prefix = "curl-7.65.3",
        urls = [
            "https://curl.haxx.se/download/curl-7.65.3.tar.gz",
        ],
    )

def _com_github_c_ares_c_ares():
    # https://github.com/grpc/grpc/blob/master/bazel/grpc_deps.bzl.
    http_archive(
        name = "com_github_c_ares_c_ares",
        build_file = "@spectator//third_party/cares:cares.BUILD",
        strip_prefix = "c-ares-1.15.0",
        sha256 = "6cdb97871f2930530c97deb7cf5c8fa4be5a0b02c7cea6e7c7667672a39d6852",
        url = "https://github.com/c-ares/c-ares/releases/download/cares-1_15_0/c-ares-1.15.0.tar.gz",
    )

def _boringssl():
    # https://github.com/tensorflow/tensorflow/blob/master/tensorflow/workspace.bzl.
    http_archive(
        # Envoy uses short name, update when switch to reverse fqdn.
        name = "boringssl",
        sha256 = "1188e29000013ed6517168600fc35a010d58c5d321846d6a6dfee74e4c788b45",
        strip_prefix = "boringssl-7f634429a04abc48e2eb041c81c5235816c96514",
        urls = ["https://github.com/google/boringssl/archive/7f634429a04abc48e2eb041c81c5235816c96514.tar.gz"],
    )

def _net_zlib():
    # https://github.com/tensorflow/tensorflow/blob/master/tensorflow/workspace.bzl.
    http_archive(
        name = "net_zlib",
        build_file = "@spectator//third_party:zlib.BUILD",
        sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
        strip_prefix = "zlib-1.2.11",
        urls = [
            "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
            "https://zlib.net/zlib-1.2.11.tar.gz",
        ],
    )

def _com_github_skarupke_flat_hash_map():
    http_archive(
        name = "com_github_skarupke_flat_hash_map",
        build_file = "@spectator//third_party:flat_hash_map.BUILD",
        sha256 = "513efb9c2f246b6df9fa16c5640618f09804b009e69c8f7bd18b3099a11203d5",
        strip_prefix = "flat_hash_map-2c4687431f978f02a3780e24b8b701d22aa32d9c",
        urls = ["https://github.com/skarupke/flat_hash_map/archive/2c4687431f978f02a3780e24b8b701d22aa32d9c.zip"],
    )

def _com_github_tessil_hopscotch_map():
    http_archive(
        name = "com_github_tessil_hopscotch_map",
        build_file = "@spectator//third_party:hopscotch_map.BUILD",
        strip_prefix = "hopscotch-map-2.2.1",
        sha256 = "73e301925e1418c5ed930ef37ebdcab2c395a6d1bdaf5a012034bb75307d33f1",
        urls = ["https://github.com/Tessil/hopscotch-map/archive/v2.2.1.tar.gz"],
    )

def _com_github_fmtlib_fmt():
    # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
    http_archive(
        name = "com_github_fmtlib_fmt",
        build_file = "@spectator//third_party:fmtlib.BUILD",
        sha256 = "4c0741e10183f75d7d6f730b8708a99b329b2f942dad5a9da3385ab92bb4a15c",
        strip_prefix = "fmt-5.3.0",
        urls = ["https://github.com/fmtlib/fmt/releases/download/5.3.0/fmt-5.3.0.zip"],
    )

def _com_github_tencent_rapidjson():
    # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
    http_archive(
        name = "com_github_tencent_rapidjson",
        build_file = "@spectator//third_party:rapidjson.BUILD",
        sha256 = "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
        strip_prefix = "rapidjson-1.1.0",
        urls = ["https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz"],
    )

def _com_github_gabime_spdlog():
    # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
    http_archive(
        name = "com_github_gabime_spdlog",
        build_file = "@spectator//third_party:spdlog.BUILD",
        sha256 = "160845266e94db1d4922ef755637f6901266731c4cb3b30b45bf41efa0e6ab70",
        strip_prefix = "spdlog-1.3.1",
        urls = ["https://github.com/gabime/spdlog/archive/v1.3.1.tar.gz"],
    )

def _com_google_googletest():
    http_archive(
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
        strip_prefix = "googletest-release-1.10.0",
        sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    )

def _com_google_googlebench():
    http_archive(
        name = "com_google_benchmark",
        urls = ["https://github.com/google/benchmark/archive/v1.5.0.tar.gz"],
        strip_prefix = "benchmark-1.5.0",
        sha256 = "3c6a165b6ecc948967a1ead710d4a181d7b0fbcaa183ef7ea84604994966221a",
    )


