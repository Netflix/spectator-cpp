config_setting(
    name = "c_ares_via_cmake",
    values = {"define": "c_ares_via_cmake=enabled"},
)

config_setting(
    name = "zlib_via_cmake",
    values = {"define": "zlib_via_cmake=enabled"},
)

cc_library(
    name = "spectator",
    srcs = glob(["spectator/*.cc"]),
    hdrs = glob(["spectator/*.h"]) + [
        "percentile_bucket_tags.inc",
        "percentile_bucket_values.inc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_fmtlib_fmt//:fmtlib",
        "@com_github_gabime_spdlog//:spdlog",
        "@com_github_skarupke_flat_hash_map//:flat_hash_map",
        "@com_github_tencent_rapidjson//:rapidjson",
        "@curl",
        "@org_exim_pcre//:pcre",
    ],
)

cc_binary(
    name = "gen_perc_bucket_tags",
    srcs = ["gen_perc_bucket_tags.cc"],
)

cc_binary(
    name = "gen_perc_bucket_values",
    srcs = ["gen_perc_bucket_values.cc"],
)

genrule(
    name = "gen_perc_bucket_tags_rule",
    outs = ["percentile_bucket_tags.inc"],
    cmd = "$(location gen_perc_bucket_tags) > $@",
    tools = [
        "gen_perc_bucket_tags",
    ],
)

genrule(
    name = "gen_perc_bucket_values_rule",
    outs = ["percentile_bucket_values.inc"],
    cmd = "$(location gen_perc_bucket_values) > $@",
    tools = [
        "gen_perc_bucket_values",
    ],
)
