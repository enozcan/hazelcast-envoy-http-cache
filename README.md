# Hazelcast Envoy Http Cache Plugin
Pluggable cache implementation backed by Hazelcast IMDG and Hazelcast Cpp Client for Envoy http cache filter.

**NOTE:** Cache filter is not ready for production (see the related PR [here](https://github.com/envoyproxy/envoy/pull/7198)),
so is not the plugin. Missing features are going to be implemented as they are done on the filter side.

## Storage

Two seperate distributed maps are used to store cached responses:

### Header Map

Stores headers of a cached response and total body size in the following form:
```
                             +------------------+
  +------------------+       | Response headers |
  | 64 bit hash key  +-----> +                  |
  +------------------+       | Trailers (soon)  |
                             |                  |
                             | Total Body Size  |
         KEY                 +------------------+
                                    VALUE
```
### Body Map

Considering the large body sizes and ranged responses, bodies are stored in partial entries on the map in order
to serve body chunks faster and ignore unnecessary bytes on a ranged response.
The partition size is configurable and can be arranged according to use case (i.e. If your responses are generally large and 
ranged requests are frequent, this will be useful). This feature can be 
removed by settting the partition size to large number of bytes. The map entries look like the following:

```
   +-----------------+-----+     +------------------+
   |str(64 bit hash) | "0" +---->+ 0 - 2 MB         |
   +-----------------------+     +------------------+
   +-----------------------+     +------------------+
   |str(64 bit hash) | "1" +---->+ 2 - 4 MB         |
   +-----------------------+     +------------------+
   +-----------------------+     +----------+
   |str(64 bit hash) | "2" +---->+ 4 - 5 MB |
   +-----------------+-----+     +----------+
            KEY                          VALUE
 ```
 The body partitions belong to the same response are stored in the same Hazelcast member using `PartitionAware`
 utility of IMDG. Hence unnecessary networking calls between cluster nodes are prevented during lookup operations. 


## Build

In the repo, Hazelcast Cpp client for OS X is included. Hence it's available only for OS X now. However, replacing the `cpp` file with
the proper distribution of Cpp client, and setting the path in `WORKSPACE` properly, the build can be compatible 
with `Windows` or `Linux`. (See Hazelcast Cpp Client documentation [here](https://github.com/hazelcast/hazelcast-cpp-client)).

(Full path to the directory must be set for `new_local_repository` in `WORKSPACE`)

```sh
$ git submodule init && git submodule init
$ bazel build --spawn_strategy=standalone //:envoy 
```

## Test & Run

Before testing starts, there must be a Hazelcast cluster running in the same host. On cache start up, 
Hazelcast Client will connect to the cluster and then it will be ready to serve the cache. Client/Server
topology details can be seen [here on Hazelcast documentation](https://docs.hazelcast.org/docs/latest/manual/html-single/#hazelcast-topology).

```sh
$ bazel test --spawn_strategy=standalone hazelcast_cache_integration_test
```

Map configurations have to be set on server side before cluster start up.
That means, cache plugin cannot set TTL, eviction percentage, eviction policy etc.
