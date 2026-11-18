load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "asio",
    hdrs = glob([
        "**/*.hpp",
        "**/*.ipp",
    ]),
    includes = ["asio/include"],
    visibility = ["//visibility:public"],
)
