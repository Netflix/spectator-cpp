load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def spectator_dependencies():
    # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
    http_archive(
        name = "com_github_fmtlib_fmt",
        build_file = "@spectator//third_party:fmtlib.BUILD",
        sha256 = "4c0741e10183f75d7d6f730b8708a99b329b2f942dad5a9da3385ab92bb4a15c",
        strip_prefix = "fmt-5.3.0",
        urls = ["https://github.com/fmtlib/fmt/releases/download/5.3.0/fmt-5.3.0.zip"],
    )

    # https://github.com/envoyproxy/envoy/blob/master/bazel/repository_locations.bzl.
    http_archive(
        name = "com_github_gabime_spdlog",
        build_file = "@spectator//third_party:spdlog.BUILD",
        sha256 = "160845266e94db1d4922ef755637f6901266731c4cb3b30b45bf41efa0e6ab70",
        strip_prefix = "spdlog-1.3.1",
        urls = ["https://github.com/gabime/spdlog/archive/v1.3.1.tar.gz"],
    )

    http_archive(
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
        strip_prefix = "googletest-release-1.10.0",
        sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    )

    http_archive(
        name = "com_google_benchmark",
        urls = ["https://github.com/google/benchmark/archive/v1.5.1.tar.gz"],
        strip_prefix = "benchmark-1.5.1",
        sha256 = "23082937d1663a53b90cb5b61df4bcc312f6dee7018da78ba00dd6bd669dfef2"
    )

    http_archive(
       name = "asio",
       build_file = "@spectator//third_party:asio.BUILD",
       urls = ["https://github.com/chriskohlhoff/asio/archive/asio-1-14-0.zip"],
       strip_prefix = "asio-asio-1-14-0",
       sha256 = "7887c91704a92dc8425385b1ff6f3e432e8869745b2ee2f896a1e7eb2a60e729",
    )

    http_archive(
       name = "com_google_absl",
       urls = ["https://github.com/abseil/abseil-cpp/archive/518f17501e6156f7921fbb9b68a1e420bcb10bc5.zip"],
       strip_prefix = "abseil-cpp-518f17501e6156f7921fbb9b68a1e420bcb10bc5",
       sha256 = "0baec77dcf13da93038ad6045c87e048a6cc1f5a8ad126091c804acab4a2671a",
    )