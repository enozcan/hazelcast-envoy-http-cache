//
// Created by Enes Özcan on 13.01.2020.
//
#include "hazelcast_http_cache.h"
#include "envoy/registry/registry.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {
namespace {

class HazelcastLookupContext : public LookupContext {

public:

  explicit HazelcastLookupContext(HazelcastHttpCache& cache,
      LookupRequest&& request) :
      hz_cache(cache),
      lookup_request(std::move(request)),
      body_partition_size(cache.bodySizePerEntry()) {
    hash_key = stableHashKey(lookup_request.key());
  }

  // Current response's hash key.
  // The key is used when storing header entries.
  inline const uint64_t& getHashKey() { return hash_key; }

  void getHeaders(LookupHeadersCallback&& cb) override {
    HazelcastHeaderPtr header_entry = hz_cache.lookupHeader(hash_key);
    if (header_entry) {
      this->total_body_size = std::move(header_entry->total_body_size);
      cb(lookup_request.makeLookupResult
        (std::move(header_entry->header_map_ptr), total_body_size));
    } else {
      cb(LookupResult{});
    }
  }

  // Hence bodies are stored partially on the cache
  // (see hazelcast_cache_entry.h for details), the
  // returning buffer from this function can have a
  // size of at most body_partition_size. Caller
  // (filter) has to check range and make another
  // getBody request if needed.
  void getBody(const AdjustedByteRange& range,
      LookupBodyCallback&& cb) override {
    ASSERT(range.end() <= total_body_size);
    uint64_t body_index = range.begin() / body_partition_size;
    HazelcastBodyPtr body = hz_cache.lookupBody
        (std::to_string(hash_key) + std::to_string(body_index));
    if (body) {
        uint64_t start = (range.begin() % body_partition_size);
        hazelcast::byte* data = body->body_buffer_.data() + start;
        if (range.end() < (body_index + 1) * body_partition_size){
          // No other chunk is needed since one chunk satisfies
          // the range. Copy only needed bytes.
          cb(std::make_unique<Buffer::OwnedImpl>(data,
              range.end() % body_partition_size));
        } else {
          // Another body chunk is needed. Hence copy all
          // the bytes until the end of the buffer.
          cb(std::make_unique<Buffer::OwnedImpl>(data,
              body->body_buffer_.size() - start));
        }
    } else {
        // Body is expected to reside in the cache but lookup is failed.
        cb(nullptr); // abort lookup
    }
  };

  void getTrailers(LookupTrailersCallback&& cb) override {
    // TODO: not supported by the filter yet.
    cb(nullptr); 
  };

private:

  HazelcastHttpCache& hz_cache;
  const LookupRequest lookup_request;

  uint64_t total_body_size; // of the current response.
  uint64_t hash_key; // of the current response.
  const uint64_t& body_partition_size; // max body size per cache entry.

};

class HazelcastInsertContext : public InsertContext {

public:

  HazelcastInsertContext(LookupContext& lookup_context,
      HazelcastHttpCache& cache) : hz_cache(cache),
      hash_key(dynamic_cast<HazelcastLookupContext&>
      (lookup_context).getHashKey()),
      body_partition_size(cache.bodySizePerEntry()) {
    available_buffer_bytes = body_partition_size;
  };

  void insertHeaders(const Http::HeaderMap& response_headers,
      bool end_stream) override {
    header.header_map_ptr =
        std::make_unique<Http::HeaderMapImpl>(response_headers);
    if (end_stream) {
      flushHeader();
    }
  }

  void insertBody(const Buffer::Instance& chunk,
      InsertCallback ready_for_next_chunk, bool end_stream) override {
    uint64_t remaining_chunk_size = chunk.length();
    uint64_t local_chunk_index = 0;
    // Insert bodies in a contiguous manner
    // using body_buffer.
    while (remaining_chunk_size) {
      ASSERT(local_chunk_index <= chunk.length());
      ASSERT(remaining_chunk_size > 0);
      if (available_buffer_bytes <= remaining_chunk_size) {
        // This chunk is going to fill the buffer, So partition is needed.
        copyIntoLocalBuffer(local_chunk_index, available_buffer_bytes, chunk);
        ASSERT(buffer_vector.size() == body_partition_size);
        remaining_chunk_size -= available_buffer_bytes;
        available_buffer_bytes = 0;
        flushBuffer();
        // TODO: Disabled for the tests temporarily:
        //if (ready_for_next_chunk) ready_for_next_chunk(false);
      } else {
        // end of the current chunk's insertion
        copyIntoLocalBuffer(local_chunk_index, remaining_chunk_size, chunk);
        available_buffer_bytes -= remaining_chunk_size;
        remaining_chunk_size = 0;
      }
    }
    
    if (end_stream) {
      // Header shouldn't be inserted before bodies to
      // ensure the total body size for this request.
      flushBuffer();
      flushHeader();
    }
    if (ready_for_next_chunk) ready_for_next_chunk(true);
  }

  void insertTrailers(const Http::HeaderMap&) override {
    // TODO: Not supported by the filter yet.
    ASSERT(false);
  };

private:

  void copyIntoLocalBuffer(uint64_t& index, uint64_t& size,
      const Buffer::Instance& source){
    uint64_t buffer_size = body_partition_size - available_buffer_bytes;
    buffer_vector.resize(buffer_vector.size() + size);
    source.copyOut(index, size, buffer_vector.data() + buffer_size);
    index += size;
  };

  void flushBuffer(){
    uint64_t buffer_size = body_partition_size - available_buffer_bytes;
    HazelcastBodyEntry bodyEntry;
    total_body_size += buffer_size;
    bodyEntry.body_buffer_ = std::move(buffer_vector);
    buffer_vector.clear();
    hz_cache.insertBody(std::to_string(hash_key) + std::to_string(body_order++),
        bodyEntry);
    available_buffer_bytes = body_partition_size; // Reset buffer index
  }

  void flushHeader(){
    header.total_body_size = total_body_size;
    hz_cache.insertHeader(hash_key, header);
  }

  HazelcastHttpCache& hz_cache;
  HazelcastHeaderEntry header;
  int body_order = 0;
  const uint64_t hash_key;
  const uint64_t& body_partition_size;
  uint64_t available_buffer_bytes;
  uint64_t total_body_size = 0;

  // Since bodies are partially stored in the cache,
  // they have to be inserted contiguous. This buffer
  // is used to store bytes coming from filter and
  // flushed when it is full.
  std::vector<hazelcast::byte> buffer_vector;

};
}

HazelcastHttpCache::HazelcastHttpCache(HazelcastConfig config)
  : hz_config_(config),
  BODY_PARTITION_SIZE(config.body_partition_size() == 0 ?
                      DEFAULT_PARTITION_SIZE :
                      config.body_partition_size()) {};

LookupContextPtr HazelcastHttpCache::
  makeLookupContext(LookupRequest&& request) {
  return std::make_unique<HazelcastLookupContext>(*this, std::move(request));
}

InsertContextPtr HazelcastHttpCache::
  makeInsertContext(LookupContextPtr&& lookup_context) {
  ASSERT(lookup_context != nullptr);
  return std::make_unique<HazelcastInsertContext>(*lookup_context, *this);
}

HazelcastBodyPtr HazelcastHttpCache::
  lookupBody(const std::string& key) {
  return hz->getMap<std::string, HazelcastBodyEntry>
      (hz_config_.body_map_name()).get(key);
}

HazelcastHeaderPtr HazelcastHttpCache::
  lookupHeader(const uint64_t& hash_key) {
  return hz->getMap<int64_t, HazelcastHeaderEntry>
      (hz_config_.header_map_name()).get(static_cast<int64_t>(hash_key));
}

void HazelcastHttpCache::insertBody(
    std::string&& hash_key, const HazelcastBodyEntry& entry) {
  hz->getMap<std::string, HazelcastBodyEntry>
      (hz_config_.body_map_name()).put(std::move(hash_key),entry);
}

void HazelcastHttpCache::insertHeader(
     const uint64_t& hash_key, const HazelcastHeaderEntry& entry) {
    hz->getMap<int64_t, HazelcastHeaderEntry>
      (hz_config_.header_map_name()).put(static_cast<int64_t>(hash_key),entry);
}

void HazelcastHttpCache::updateHeaders(LookupContextPtr&& lookup_context,
                                       Http::HeaderMapPtr&& response_headers) {
  // TODO: Not supported by the filter yet.
  ASSERT(lookup_context);
  ASSERT(response_headers);
  ASSERT(false);
}

CacheInfo HazelcastHttpCache::cacheInfo() const {
  CacheInfo cache_info;
  cache_info.name_ = "envoy.extensions.http.cache.hazelcast";
  cache_info.supports_range_requests_ = true;
  return cache_info;
}

inline const uint64_t& HazelcastHttpCache::bodySizePerEntry(){
  return BODY_PARTITION_SIZE;
}
void HazelcastHttpCache::clearMaps() {
  hz->getMap<std::string, HazelcastBodyEntry>
      (hz_config_.body_map_name()).clear();
  hz->getMap<std::string, HazelcastHeaderEntry>
      (hz_config_.header_map_name()).clear();
}
void HazelcastHttpCache::connect() {
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

void HazelcastHttpCache::disconnect() {
  if (hz) {
    hz->shutdown();
    hz.release();
  }
}

HazelcastHttpCache::~HazelcastHttpCache() {
  disconnect();
}

class HazelcastHttpCacheFactory : public HttpCacheFactory {
public:
  HazelcastHttpCacheFactory() : HttpCacheFactory("envoy.extensions.http.cache.hazelcast") {};

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<Envoy::ProtobufWkt::Empty>();
  }

  HttpCache& getCache(
      const envoy::config::filter::http::cache::v2::CacheConfig& cache_config) override {
    HazelcastConfig hz_config;
    MessageUtil::unpackTo(cache_config.typed_config(), hz_config);
    cache_ = std::make_unique<HazelcastHttpCache>(hz_config);
    cache_->connect();
    return *cache_;
  }

private:
  std::unique_ptr<HazelcastHttpCache> cache_;

};

static Registry::RegisterFactory<HazelcastHttpCacheFactory, HttpCacheFactory> register_;

} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
