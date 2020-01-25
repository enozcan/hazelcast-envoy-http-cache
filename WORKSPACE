workspace(name = "hazelcast_http_cache")

local_repository(
    name = "envoy",
    path = "envoy",
)

load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()

new_local_repository(
    name = "hazelcast",
    path = "/path/to/dir/cpp",
    build_file = "/path/to/dir/cpp/hazelcast.BUILD",
)
