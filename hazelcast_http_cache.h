//
// Created by Enes Özcan on 13.01.2020.
//
#pragma once

#include "extensions/filters/http/cache/http_cache.h"
#include "hazelcast_cluster_service.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {

class HazelcastHttpCache : public HttpCache {

public:
  HazelcastHttpCache(HazelcastClusterService& cs);

  // Cache::HttpCache
  LookupContextPtr makeLookupContext(LookupRequest&& request) override;
  InsertContextPtr makeInsertContext
    (LookupContextPtr&& lookup_context) override;
  void updateHeaders(LookupContextPtr&& lookup_context,
      Http::HeaderMapPtr&& response_headers) override;
  CacheInfo cacheInfo() const override;


  void insertHeader(const uint64_t& hash_key, const HazelcastHeaderEntry& entry);
  void insertBody(std::string&& hash_key, const HazelcastBodyEntry& entry);
  HazelcastHeaderPtr lookupHeader(const uint64_t& hash_key);
  HazelcastBodyPtr lookupBody(const std::string& key);
  const uint64_t& bodySizePerEntry();
  void clearMaps(); // For testing only

private:
  HazelcastClusterService& cluster_service;
  const uint64_t BODY_PARTITION_SIZE;
};

} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
