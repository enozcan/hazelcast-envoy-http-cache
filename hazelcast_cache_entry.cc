//
// Created by Enes Özcan on 21.01.2020.
//
#include "hazelcast_cache_entry.h"

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
        absl::string_view key_view = header.key().getStringView();
        absl::string_view val_view = header.value().getStringView();
        std::vector<char> key_vector(key_view.begin(),key_view.end());
        std::vector<char> val_vector(val_view.begin(),val_view.end());
        writer_ptr->writeCharArray(&key_vector);
        writer_ptr->writeCharArray(&val_vector);
        return Http::HeaderMap::Iterate::Continue;
      },
      &writer);
  writer.writeLong(total_body_size);
}

void HazelcastHeaderEntry::readData(ObjectDataInput &reader) {
  header_map_ptr = std::make_unique<Http::HeaderMapImpl>();
  int headers_size = reader.readInt();
  for (int i = 0; i < headers_size; i++) {
    std::vector<char> key_vector = *reader.readCharArray();
    std::vector<char> val_vector = *reader.readCharArray();
    Http::HeaderString key,val;
    key.append(key_vector.data(),key_vector.size());
    val.append(val_vector.data(),val_vector.size());
    header_map_ptr->addViaMove(std::move(key),std::move(val));
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
