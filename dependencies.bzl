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

# We feel like this is too fragile, complex and verbose and just sandbox all dependencies by prefixing them thus hiding
# from the consumers and avoiding any possible name conflicts.
# This will possibly change once Bazel comes up with a better standardized solution but should suffice for now.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def spectator_dependencies():
    http_archive(
        # Does not build with a different name right now.
        name = "curl",
        build_file = "@spectator//third_party:curl.BUILD",
        sha256 = "e9c37986337743f37fd14fe8737f246e97aec94b39d1b71e8a5973f72a9fc4f5",
        strip_prefix = "curl-7.60.0",
        urls = [
            "https://mirror.bazel.build/curl.haxx.se/download/curl-7.60.0.tar.gz",
            "https://curl.haxx.se/download/curl-7.60.0.tar.gz",
        ],
    )

    # curl dependency.
    http_archive(
        name = "spectator_boringssl",
        # Use commits from branch "chromium-stable-with-bazel"
        sha256 = "a4a71d97b90825f509c472cc9ad2404d4100f6cce042fc159388956bc5c616fb",
        strip_prefix = "boringssl-77e47de9e16ec8865d1bc6d614dd918141f094d2",
        # chromium-71.0.3578.80
        urls = ["https://github.com/google/boringssl/archive/77e47de9e16ec8865d1bc6d614dd918141f094d2.tar.gz"],
    )

    # curl dependency.
    http_archive(
        name = "spectator_zlib",
        build_file = "@spectator//third_party:zlib.BUILD",
        sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
        strip_prefix = "zlib-1.2.11",
        urls = [
            "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
            "https://zlib.net/zlib-1.2.11.tar.gz",
        ],
    )

    http_archive(
        name = "spectator_flat_hash_map",
        build_file = "@spectator//third_party:flat_hash_map.BUILD",
        sha256 = "513efb9c2f246b6df9fa16c5640618f09804b009e69c8f7bd18b3099a11203d5",
        strip_prefix = "flat_hash_map-2c4687431f978f02a3780e24b8b701d22aa32d9c",
        urls = ["https://github.com/skarupke/flat_hash_map/archive/2c4687431f978f02a3780e24b8b701d22aa32d9c.zip"],
    )

    http_archive(
        name = "spectator_fmtlib",
        build_file = "@spectator//third_party:fmtlib.BUILD",
        sha256 = "43894ab8fe561fc9e523a8024efc23018431fa86b95d45b06dbe6ddb29ffb6cd",
        strip_prefix = "fmt-5.2.1",
        urls = ["https://github.com/fmtlib/fmt/releases/download/5.2.1/fmt-5.2.1.zip"],
    )

    http_archive(
        name = "spectator_pcre",
        build_file = "@spectator//third_party:pcre.BUILD",
        sha256 = "69acbc2fbdefb955d42a4c606dfde800c2885711d2979e356c0636efde9ec3b5",
        strip_prefix = "pcre-8.42",
        urls = [
            "https://mirror.bazel.build/ftp.exim.org/pub/pcre/pcre-8.42.tar.gz",
            "http://ftp.exim.org/pub/pcre/pcre-8.42.tar.gz",
        ],
    )

    http_archive(
        name = "spectator_rapidjson",
        build_file = "@spectator//third_party:rapidjson.BUILD",
        sha256 = "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
        strip_prefix = "rapidjson-1.1.0",
        urls = ["https://github.com/Tencent/rapidjson/archive/v1.1.0.tar.gz"],
    )

    http_archive(
        name = "spectator_spdlog",
        build_file = "@spectator//third_party:spdlog.BUILD",
        sha256 = "867a4b7cedf9805e6f76d3ca41889679054f7e5a3b67722fe6d0eae41852a767",
        strip_prefix = "spdlog-1.2.1",
        urls = ["https://github.com/gabime/spdlog/archive/v1.2.1.tar.gz"],
    )

    http_archive(
        name = "spectator_googletest",
        sha256 = "9bf1fe5182a604b4135edc1a425ae356c9ad15e9b23f9f12a02e80184c3a249c",
        strip_prefix = "googletest-release-1.8.1",
        urls = ["https://github.com/google/googletest/archive/release-1.8.1.tar.gz"],
    )
