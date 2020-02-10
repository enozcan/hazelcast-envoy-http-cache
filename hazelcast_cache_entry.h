//
// Created by Enes Özcan on 21.01.2020.
//
#pragma once

#include "common/buffer/buffer_impl.h"
#include "common/http/header_map_impl.h"
#include "hazelcast/client/serialization/IdentifiedDataSerializable.h"
#include "hazelcast/client/serialization/ObjectDataInput.h"
#include "hazelcast/client/serialization/ObjectDataOutput.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {

static const int HAZELCAST_BODY_TYPE_ID = 100;
static const int HAZELCAST_HEADER_TYPE_ID = 101;
static const int HAZELCAST_ENTRY_SERIALIZER_FACTORY_ID = 1000;

using BufferImplPtr = std::unique_ptr<Buffer::OwnedImpl>;
using hazelcast::client::serialization::IdentifiedDataSerializable;
using hazelcast::client::serialization::ObjectDataOutput;
using hazelcast::client::serialization::ObjectDataInput;
using hazelcast::client::serialization::DataSerializableFactory;

/**
 *  Structure for cached response headers.
 *
 *  Stored on Hazelcast IMap in key value format. Key is
 *  created by Filters::Http:Cache::stableHashKey and the
 *  value is the entry itself containing header maps and
 *  total body size of the cached response.
 *
 *  Header Map content is written to distributed map entry
 *  during serialization in K,V format respectively for
 *  each HeaderEntry.
 *
 *  The distributed map looks like the below:
 *
 *                         +------------------+
 *  +--------------+       | Response headers |
 *  | 64 bit hash  +-----> +                  |
 *  +--------------+       | Total Body Size  |
 *         KEY             +------------------+
 *                                 VALUE
 */
class HazelcastHeaderEntry : public IdentifiedDataSerializable {
public:
  static const int TYPE_ID = HAZELCAST_HEADER_TYPE_ID;
  Http::HeaderMapImplPtr header_map_ptr;
  uint64_t total_body_size;

  HazelcastHeaderEntry();
  HazelcastHeaderEntry(const HazelcastHeaderEntry &other);

  // serialization::IdentifiedDataSerializable
  int getFactoryId() const;
  int getClassId() const;
  void writeData(ObjectDataOutput &writer) const;
  void readData(ObjectDataInput &reader);

};

/**
 * Structure for cached response bodies.
 *
 * For both keeping entry sizes on distributed map reasonable and
 * making ranged based responses more efficient, response body is
 * cached as partitions. Say a response has 5 MB response body and
 * BODY_PARTITION_SIZE is set as 2 MB. Then this response will have
 * 3 different entries on cache such that:
 *
 * +-----------------+-----+     +------------------+
 * |str(64 bit hash) | "0" +---->+ 0 - 2 MB         |
 * +-----------------------+     +------------------+
 * +-----------------------+     +------------------+
 * |str(64 bit hash) | "1" +---->+ 2 - 4 MB         |
 * +-----------------------+     +------------------+
 * +-----------------------+     +----------+
 * |str(64 bit hash) | "2" +---->+ 4 - 5 MB |
 * +-----------------+-----+     +----------+
 *          KEY                          VALUE
 *
 * 64 bit hash keys here come from the same origin as in header map.
 *
 * This operation comes with the cost of increased entry sizes (fixed
 * cost for map entries). However, upon a ranged request it makes
 * cache response faster. This trade off is up to user.
 *
 * This is an optional feature and if partition size is set a large
 * number, the cache will operate as it stores bodies without
 * partitioning.
 *
 */

// TODO: Implement Hazelcast::PartitionAware for body entries.
//  Hence all related bodies are stored in the same Hazelcast
//  node.
class HazelcastBodyEntry : public IdentifiedDataSerializable {
public:
  static const int TYPE_ID = HAZELCAST_BODY_TYPE_ID;

  std::vector<hazelcast::byte> body_buffer_;

  HazelcastBodyEntry();
  HazelcastBodyEntry(const HazelcastBodyEntry &other);

  // serialization::IdentifiedDataSerializable
  int getFactoryId() const;
  int getClassId() const;
  void writeData(ObjectDataOutput& writer) const;
  void readData(ObjectDataInput &reader);

};

// To make cache compatible with Hazelcast Cpp Client,
// boost pointers are used internally instead of std.
using HazelcastHeaderPtr = boost::shared_ptr<HazelcastHeaderEntry>;
using HazelcastBodyPtr = boost::shared_ptr<HazelcastBodyEntry>;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Hazelcast Cpp Client uses std::auto_ptr internally.
// Hence the warnings are suppressed here.

class HazelcastCacheEntrySerializableFactory : public DataSerializableFactory {

public:

  static const int FACTORY_ID = HAZELCAST_ENTRY_SERIALIZER_FACTORY_ID;

  virtual std::auto_ptr<IdentifiedDataSerializable> create(int32_t classId) {
    switch (classId) {
    case HAZELCAST_BODY_TYPE_ID:
      return std::auto_ptr<IdentifiedDataSerializable>
          (new HazelcastBodyEntry());
    case HAZELCAST_HEADER_TYPE_ID:
      return std::auto_ptr<IdentifiedDataSerializable>
          (new HazelcastHeaderEntry());
    default:
      return std::auto_ptr<IdentifiedDataSerializable>();
    }
  }
};

#pragma GCC diagnostic pop

} // Cache
} // HttpFilters
} // Extensions
} // Envoy



