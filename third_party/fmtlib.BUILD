# https://github.com/envoyproxy/envoy/blob/master/bazel/external/fmtlib.BUILD.

cc_library(
    name = "fmtlib",
    srcs = glob([
        "fmt/*.cc",
    ]),
    hdrs = glob([
        "include/fmt/*.h",
    ]),
    defines = ["FMT_HEADER_ONLY"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)
