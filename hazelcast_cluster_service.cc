//
// Created by Enes Özcan on 21.01.2020.
//

#include "hazelcast_cluster_service.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {

// Hard coded config for the ease of development.
// TODO: Replaced with file based configuration.
HazelcastCacheConfig cacheConfig;

HazelcastClusterService::HazelcastClusterService() {
  ClientConfig config;
  config.getGroupConfig().setName(cacheConfig.HZ_GROUP_NAME);
  config.getNetworkConfig().addAddress(
      hazelcast::client::Address(cacheConfig.HZ_CLUSTER_IP,
          cacheConfig.HZ_CLUSTER_PORT));
  config.getSerializationConfig().addDataSerializableFactory(
      HazelcastCacheEntrySerializableFactory::FACTORY_ID,
      boost::shared_ptr<serialization::DataSerializableFactory>
          (new HazelcastCacheEntrySerializableFactory()));
  hz = std::make_unique<HazelcastClient>(config);
}

// TODO: Make local refs for the maps.
//  calling getMap at each operation might be costly.

void HazelcastClusterService::
  insertHeader(std::string &&key, const HazelcastHeaderEntry &value) {
  hz->getMap<std::string, HazelcastHeaderEntry>
      (cacheConfig.HZ_HEADER_MAP_NAME).put(std::move(key),value);
}

void HazelcastClusterService::
  insertBody(std::string &&key, const HazelcastBodyEntry &value) {
  hz->getMap<std::string, HazelcastBodyEntry>
      (cacheConfig.HZ_BODY_MAP_NAME).put(std::move(key),value);
}

HazelcastHeaderPtr HazelcastClusterService::
  lookupHeader(const std::string &key) {
  return hz->getMap<std::string, HazelcastHeaderEntry>
      (cacheConfig.HZ_HEADER_MAP_NAME).get(key);
}

HazelcastBodyPtr HazelcastClusterService::
  lookupBody(const std::string &key){
  return hz->getMap<std::string, HazelcastBodyEntry>
      (cacheConfig.HZ_BODY_MAP_NAME).get(key);
}
void HazelcastClusterService::clearMaps() {
  hz->getMap<std::string, HazelcastBodyEntry>(cacheConfig.HZ_BODY_MAP_NAME)
      .clear();
  hz->getMap<std::string, HazelcastHeaderEntry>(cacheConfig.HZ_HEADER_MAP_NAME)
      .clear();
}

} // Cache
} // HttpFilters
} // Extensions
} // Envoy



