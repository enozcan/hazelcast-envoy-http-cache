//
// Created by Enes Özcan on 21.01.2020.
//
#include "hazelcast_cache_entry.h"
#include "absl/container/fixed_array.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {

/// HazelcastHeaderHeaderEntry

HazelcastHeaderEntry::HazelcastHeaderEntry() {};

int HazelcastHeaderEntry::getClassId() const {
  return TYPE_ID;
}

int HazelcastHeaderEntry::getFactoryId() const {
  return HAZELCAST_ENTRY_SERIALIZER_FACTORY_ID;
}

void HazelcastHeaderEntry::writeData(ObjectDataOutput &writer) const {
  writer.writeInt(header_map_ptr->size());
  header_map_ptr->iterate(
      [](const Http::HeaderEntry& header, void* context) ->
      Http::HeaderMap::Iterate {ObjectDataOutput* writer_ptr =
            static_cast<ObjectDataOutput*>(context);
        // TODO: May be prevent copying via writing bytes
        absl::string_view key_view = header.key().getStringView();
        absl::string_view val_view = header.value().getStringView();
        std::string K(key_view.begin(), key_view.size());
        std::string V(val_view.begin(), val_view.size());
        writer_ptr->writeUTF(&K);
        writer_ptr->writeUTF(&V);
        return Http::HeaderMap::Iterate::Continue;
      },
      &writer);
  writer.writeLong(total_body_size);
}

void HazelcastHeaderEntry::readData(ObjectDataInput &reader) {
  header_map_ptr = std::make_unique<Http::HeaderMapImpl>();
  int headers_size = reader.readInt();
  for (int i = 0; i < headers_size; i++) {
    // TODO: May be use HeaderMapImpl::addViaMove and prevent copy
    Http::LowerCaseString K(*reader.readUTF());
    std::string V(*reader.readUTF());
    header_map_ptr->addCopy(K,V);
  }
  total_body_size = reader.readLong();
}

// Hazelcast needs copy constructor in case of Near Cache usage.
HazelcastHeaderEntry::HazelcastHeaderEntry(const HazelcastHeaderEntry &other) {
  this->total_body_size = other.total_body_size;
  other.header_map_ptr->iterate(
      [](const Http::HeaderEntry& header, void* context) ->
      Http::HeaderMap::Iterate {
        Http::HeaderString key_string;
        key_string.setCopy(header.key().getStringView());
        Http::HeaderString value_string;
        value_string.setCopy(header.value().getStringView());
        static_cast<HazelcastHeaderEntry*>(context)->header_map_ptr->
          addViaMove(std::move(key_string), std::move(value_string));
        return Http::HeaderMap::Iterate::Continue;
      },
      this);
}

/// HazelcastBodyEntry

HazelcastBodyEntry::HazelcastBodyEntry() {};

// Hazelcast needs copy constructor in case of Near Cache usage.
HazelcastBodyEntry::HazelcastBodyEntry(const HazelcastBodyEntry &other) {
  this->body_buffer_ = other.body_buffer_;
};

int HazelcastBodyEntry::getFactoryId() const {
  return HAZELCAST_ENTRY_SERIALIZER_FACTORY_ID;
};

int HazelcastBodyEntry::getClassId() const {
  return TYPE_ID;
};

void HazelcastBodyEntry::writeData(ObjectDataOutput &writer) const {
  writer.writeByteArray(&body_buffer_);
}

void HazelcastBodyEntry::readData(ObjectDataInput &reader) {
  body_buffer_ = *reader.readByteArray();
}

} // Cache
} // HttpFilters
} // Extensions
} // Envoy
