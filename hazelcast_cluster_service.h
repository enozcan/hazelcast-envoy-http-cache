//
// Created by Enes Özcan on 21.01.2020.
//
#pragma once

#include "hazelcast/client/HazelcastClient.h"
#include "hazelcast/client/IMap.h"
#include "hazelcast_cache_entry.h"
#include "hazelcast.pb.h"

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
  HazelcastClusterService(HazelcastConfig hz_config);
  HazelcastHeaderPtr lookupHeader(const std::string &key);
  HazelcastBodyPtr lookupBody(const std::string &key);
  void insertBody(std::string &&key, const HazelcastBodyEntry &value);
  void insertHeader(std::string &&key, const HazelcastHeaderEntry &value);
  void connect();
  void clearMaps(); // for testing only, to be removed.
  uint64_t partitionSize();
private:
  HazelcastConfig hz_config_;
  std::unique_ptr<HazelcastClient> hz;
  const uint64_t DEFAULT_PARTITION_SIZE  = 1024;
};

} // Cache
} // HttpFilters
} // Extensions
} // Envoy