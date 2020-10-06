load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def spectator_dependencies():
    http_archive(
        name = "com_github_fmtlib_fmt",
        build_file = "@spectator//third_party:fmtlib.BUILD",
        strip_prefix = "fmt-7.0.3",
        sha256 = "decfdf9ad274070fa85f26407b816f5a4d82205ae86bac1990be658d0795ea4d",
        urls = ["https://github.com/fmtlib/fmt/releases/download/7.0.3/fmt-7.0.3.zip"],
    )

    http_archive(
        name = "com_github_gabime_spdlog",
        build_file = "@spectator//third_party:spdlog.BUILD",
        strip_prefix = "spdlog-1.8.0",
        sha256 = "1e68e9b40cf63bb022a4b18cdc1c9d88eb5d97e4fd64fa981950a9cacf57a4bf",
        urls = ["https://github.com/gabime/spdlog/archive/v1.8.0.tar.gz"],
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
