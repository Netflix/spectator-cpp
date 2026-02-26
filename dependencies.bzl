load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def spectator_dependencies():
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
