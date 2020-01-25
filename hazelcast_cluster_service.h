//
// Created by Enes Özcan on 21.01.2020.
//
#pragma once

#include "hazelcast/client/HazelcastClient.h"
#include "hazelcast/client/IMap.h"
#include "hazelcast_cache_entry.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {

using hazelcast::client::IMap;

/**
 * Wrapper class for Hazelcast cluster connection.
 * All distributed map lookup and insert operations
 * are performed here.
 */

// TODO: Add checking mechanism for cluster connection.
//  Make service unavailable if the connection is lost
//  and run connection restoration worker.
class HazelcastClusterService {
public:
  HazelcastClusterService();
  HazelcastHeaderPtr lookupHeader(const std::string &key);
  HazelcastBodyPtr lookupBody(const std::string &key);
  void insertBody(std::string &&key, const HazelcastBodyEntry &value);
  void insertHeader(std::string &&key, const HazelcastHeaderEntry &value);
  void clearMaps(); // for testing only, to be removed.
private:
  std::unique_ptr<HazelcastClient> hz;
};

/**
 * Hardcoded config values for testing purposes only.
 */
typedef struct {
  // TODO: Configure via config file
  const std::string HZ_GROUP_NAME = "envoy";
  const std::string HZ_CLUSTER_IP = "127.0.0.1";
  const int HZ_CLUSTER_PORT = 5701;
  const std::string HZ_BODY_MAP_NAME = "hz::cache::body";
  const std::string HZ_HEADER_MAP_NAME = "hz::cache::header";
  const uint64_t HZ_BODY_PARTITION_SIZE = 32;
} HazelcastCacheConfig;

} // Cache
} // HttpFilters
} // Extensions
} // Envoy