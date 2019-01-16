cc_library(
    name = "spectator",
    srcs = glob(["spectator/*.cc"]),
    hdrs = glob(["spectator/*.h"]),
    visibility = ["//visibility:public"],
    deps = [
        "@curl",
        "@spectator_flat_hash_map//:flat_hash_map",
        "@spectator_fmtlib//:fmtlib",
        "@spectator_pcre//:pcre",
        "@spectator_rapidjson//:rapidjson",
        "@spectator_spdlog//:spdlog",
    ],
)
