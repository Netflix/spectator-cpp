load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def spectator_dependencies():
    http_archive(
        name = "com_github_fmtlib_fmt",
        build_file = "@spectator//third_party:fmtlib.BUILD",
        strip_prefix = "fmt-8.0.1",
        sha256 = "a627a56eab9554fc1e5dd9a623d0768583b3a383ff70a4312ba68f94c9d415bf",
        urls = ["https://github.com/fmtlib/fmt/releases/download/8.0.1/fmt-8.0.1.zip"],
    )

    http_archive(
        name = "com_github_gabime_spdlog",
        build_file = "@spectator//third_party:spdlog.BUILD",
        strip_prefix = "spdlog-1.9.1",
        sha256 = "9a452cfa24408baccc9b2bc2d421d68172a7630c99e9504a14754be840d31a62",
        urls = ["https://github.com/gabime/spdlog/archive/v1.9.1.tar.gz"],
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
        sha256 = "23082937d1663a53b90cb5b61df4bcc312f6dee7018da78ba00dd6bd669dfef2",
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

    http_archive(
        name = "com_github_bombela_backward",
        urls = ["https://github.com/bombela/backward-cpp/archive/1efdd145b5fa84f457fb6727677ce0bc9f2c7b5b.zip"],
        strip_prefix = "backward-cpp-1efdd145b5fa84f457fb6727677ce0bc9f2c7b5b",
        build_file = "@spectator//third_party:backward.BUILD",
        sha256 = "97ddc265cc42afadf870ccfa9b079382766eb9e46e8fc1994a61a92ff547c851",
    )

    http_archive(
        name = "com_grail_bazel_compdb",
        strip_prefix = "bazel-compilation-database-71edeae2eda57e4be679394d22b8ef7a60ca3860",
        sha256 = "7504feffad927af7ba54213e741975b4e1e890c6cc7d1bc186ea4024508acd4c",
        urls = ["https://github.com/grailbio/bazel-compilation-database/archive/71edeae2eda57e4be679394d22b8ef7a60ca3860.zip"],
    )
