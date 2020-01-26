//
// Created by Enes Özcan on 21.01.2020.
//

#include "hazelcast_cluster_service.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {

HazelcastClusterService::HazelcastClusterService(HazelcastConfig hz_config) :
  hz_config_(hz_config) {};

void HazelcastClusterService::connect() {

  if (hz) return;

  ClientConfig config;
  config.getGroupConfig().setName(hz_config_.group_name());

  config.getNetworkConfig().addAddress(
      hazelcast::client::Address(hz_config_.ip(),hz_config_.port()));

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
      (hz_config_.header_map_name()).put(std::move(key),value);
}

void HazelcastClusterService::
  insertBody(std::string &&key, const HazelcastBodyEntry &value) {
  hz->getMap<std::string, HazelcastBodyEntry>
      (hz_config_.body_map_name()).put(std::move(key),value);
}

HazelcastHeaderPtr HazelcastClusterService::
  lookupHeader(const std::string &key) {
  return hz->getMap<std::string, HazelcastHeaderEntry>
      (hz_config_.header_map_name()).get(key);
}

HazelcastBodyPtr HazelcastClusterService::
  lookupBody(const std::string &key){
  return hz->getMap<std::string, HazelcastBodyEntry>
      (hz_config_.body_map_name()).get(key);
}
void HazelcastClusterService::clearMaps() {
  hz->getMap<std::string, HazelcastBodyEntry>(hz_config_.body_map_name())
      .clear();
  hz->getMap<std::string, HazelcastHeaderEntry>(hz_config_.header_map_name())
      .clear();
}
uint64_t HazelcastClusterService::partitionSize() {
  return hz_config_.body_partition_size() > 0 ?
      hz_config_.body_partition_size() : DEFAULT_PARTITION_SIZE;
}

} // Cache
} // HttpFilters
} // Extensions
} // Envoy



