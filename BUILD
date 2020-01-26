package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_proto_library",
)

envoy_proto_library(
    name = "hazelcast",
    srcs = ["hazelcast.proto"],
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":hazelcast_http_cache_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

envoy_cc_library(
    name = "hazelcast_http_cache_lib",
    srcs = ["hazelcast_http_cache.cc"],
    hdrs = ["hazelcast_http_cache.h"],
    repository = "@envoy",
    deps = [
        ":hazelcast_cluster_service_lib",
        "@envoy//include/envoy/registry",
        "@envoy//source/extensions/filters/http/cache:http_cache_lib",
    ],
)


envoy_cc_library(
    name = "hazelcast_cluster_service_lib",
    srcs = ["hazelcast_cluster_service.cc"],
    hdrs = ["hazelcast_cluster_service.h"],
    repository = "@envoy",
    deps = [
        ":hazelcast_cc_proto",
        ":hazelcast_cache_entry_lib",
    ],
)

envoy_cc_library(
    name = "hazelcast_cache_entry_lib",
    srcs = ["hazelcast_cache_entry.cc"],
    hdrs = ["hazelcast_cache_entry.h"],
    repository = "@envoy",
    deps = [
        "@hazelcast//:client",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/buffer:buffer_lib",
    ],
)

envoy_cc_test(
    name = "hazelcast_cache_integration_test",
    srcs = ["hazelcast_http_cache_test.cc"],
    repository = "@envoy",
    deps = [
        ":hazelcast_http_cache_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

