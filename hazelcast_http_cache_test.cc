/**
 * NOTE: These tests belong to the the simple cache implementation on
 * https://github.com/toddmgreer/envoy/blob/master/test/extensions/filters/http/cache/simple_http_cache_test.cc
 * Here, the caching provider is set as Hazelcast IMDG and tested accordingly
 * with minor changes on the test body.
 *
 */
#include "envoy/registry/registry.h"
#include "test/test_common/simulated_time_system.h"
#include "test/test_common/utility.h"
#include "gtest/gtest.h"
#include "hazelcast_http_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Cache {
namespace {

class HazelcastHttpCacheTest : public testing::Test {
protected:

  static HazelcastClusterService* cs;

  HazelcastHttpCacheTest() {
    hz_cache_ptr = std::make_unique<HazelcastHttpCache>(*cs,
        3);
    hz_cache_ptr->clearMaps();
    request_headers_.setMethod("GET");
    request_headers_.setHost("example.com");
    request_headers_.setForwardedProto("https");
    request_headers_.setCacheControl("max-age=3600");
  }

  static void SetUpTestSuite() {
    cs = new HazelcastClusterService();
  }

  static void TearDownTestSuite() {
    delete cs;
    cs = nullptr;
  }

  // Performs a cache lookup.
  LookupContextPtr lookup(absl::string_view request_path) {
    LookupRequest request = makeLookupRequest(request_path);
    LookupContextPtr context = hz_cache_ptr->makeLookupContext
        (std::move(request));
    context->getHeaders([this](LookupResult&& result) {
      lookup_result_ = std::move(result); });
    return context;
  }

  // Inserts a value into the cache.
  void insert(LookupContextPtr lookup,
      const Http::TestHeaderMapImpl& response_headers,
      const absl::string_view response_body) {
    InsertContextPtr inserter = hz_cache_ptr->makeInsertContext(move(lookup));
    inserter->insertHeaders(response_headers, false);
    inserter->insertBody(Buffer::OwnedImpl(response_body), nullptr, true);
  }

  void insert(absl::string_view request_path,
      const Http::TestHeaderMapImpl& response_headers,
      const absl::string_view response_body) {
    insert(lookup(request_path), response_headers, response_body);
  }

  std::string getBody(LookupContext& context, uint64_t start, uint64_t end) {
    std::string full_body;
    std::string body_chunk;
    uint64_t start_ = start;

    while (full_body.length() != end - start) {
      AdjustedByteRange range(start_, end);
      context.getBody(range, [&body_chunk, &start_,
                              &full_body](Buffer::InstancePtr&& data) {
        EXPECT_NE(data, nullptr);
        if (data) {
          body_chunk = data->toString();
          full_body.append(body_chunk);
          start_ += body_chunk.length();
        }
      });
    }
    return full_body;
  }

  LookupRequest makeLookupRequest(absl::string_view request_path) {
    request_headers_.setPath(request_path);
    return LookupRequest(request_headers_, current_time_);
  }

  AssertionResult expectLookupSuccessWithBody(LookupContext* lookup_context,
                                              absl::string_view body) {
    if (lookup_result_.cache_entry_status_ != CacheEntryStatus::Ok) {
      return AssertionFailure() << "Expected: lookup_result_.cache_entry_status"
                                   " == CacheEntryStatus::Ok\n  Actual: "
                                << lookup_result_.cache_entry_status_;
    }
    if (!lookup_result_.headers_) {
      return AssertionFailure() << "Expected nonnull lookup_result_.headers";
    }
    if (!lookup_context) {
      return AssertionFailure() << "Expected nonnull lookup_context";
    }
    const std::string actual_body = getBody(*lookup_context, 0, body.size());
    if (body != actual_body) {
      return AssertionFailure() << "Expected body == " << body <<
      "\n  Actual:  " << actual_body;
    }
    return AssertionSuccess();
  }

  std::unique_ptr<HazelcastHttpCache> hz_cache_ptr;
  LookupResult lookup_result_;
  Http::TestHeaderMapImpl request_headers_;
  Event::SimulatedTimeSystem time_source_;
  SystemTime current_time_ = time_source_.systemTime();
  DateFormatter formatter_{"%a, %d %b %Y %H:%M:%S GMT"};
};

HazelcastClusterService* HazelcastHttpCacheTest::cs = nullptr;

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(HazelcastHttpCacheTest, PutGet) {

  const std::string RequestPath1("Name");
  LookupContextPtr name_lookup_context = lookup(RequestPath1);
  EXPECT_EQ(CacheEntryStatus::Unusable, lookup_result_.cache_entry_status_);

  Http::TestHeaderMapImpl response_headers{
    {"date", formatter_.fromTime(current_time_)},
    {"cache-control", "public,max-age=3600"}};

  const std::string Body1("Value");
  insert(move(name_lookup_context), response_headers, Body1);
  name_lookup_context = lookup(RequestPath1);
  EXPECT_TRUE(expectLookupSuccessWithBody(name_lookup_context.get(), Body1));

  const std::string& RequestPath2("Another Name");
  LookupContextPtr another_name_lookup_context = lookup(RequestPath2);
  EXPECT_EQ(CacheEntryStatus::Unusable, lookup_result_.cache_entry_status_);

  const std::string NewBody1("NewValue");
  insert(move(name_lookup_context), response_headers, NewBody1);
  EXPECT_TRUE(expectLookupSuccessWithBody(lookup(RequestPath1).get(),
      NewBody1));
  hz_cache_ptr->clearMaps();
}

TEST_F(HazelcastHttpCacheTest, PrivateResponse) {
  Http::TestHeaderMapImpl response_headers{
    {"date", formatter_.fromTime(current_time_)},
    {"age", "2"},
    {"cache-control", "private,max-age=3600"}};
  const std::string request_path("Name");

  LookupContextPtr name_lookup_context = lookup(request_path);
  EXPECT_EQ(CacheEntryStatus::Unusable, lookup_result_.cache_entry_status_);

  const std::string Body("Value");
  // We must make sure at cache insertion time, private responses must not be
  // inserted. However, if the insertion did happen, it would be served at the
  // time of lookup.
  insert(move(name_lookup_context), response_headers, Body);
  EXPECT_TRUE(expectLookupSuccessWithBody(lookup(request_path).get(), Body));
  hz_cache_ptr->clearMaps();
}

TEST_F(HazelcastHttpCacheTest, Miss) {
  LookupContextPtr name_lookup_context = lookup("Name");
  EXPECT_EQ(CacheEntryStatus::Unusable, lookup_result_.cache_entry_status_);
}

TEST_F(HazelcastHttpCacheTest, Fresh) {
  const Http::TestHeaderMapImpl response_headers = {
      {"date", formatter_.fromTime(current_time_)},
      {"cache-control", "public, max-age=3600"}};
  insert("/", response_headers, "");
  time_source_.sleep(std::chrono::seconds(3600));
  lookup("/");
  EXPECT_EQ(CacheEntryStatus::Ok, lookup_result_.cache_entry_status_);
  hz_cache_ptr->clearMaps();
}

TEST_F(HazelcastHttpCacheTest, Stale) {
  const Http::TestHeaderMapImpl response_headers = {
      {"date", formatter_.fromTime(current_time_)},
      {"cache-control", "public, max-age=3600"}};
  insert("/", response_headers, "");
  time_source_.sleep(std::chrono::seconds(3601));
  lookup("/");
  EXPECT_EQ(CacheEntryStatus::Ok, lookup_result_.cache_entry_status_);
  hz_cache_ptr->clearMaps();
}
TEST_F(HazelcastHttpCacheTest, RequestSmallMinFresh) {
  request_headers_.setReferenceKey(Http::Headers::get().CacheControl,
      "min-fresh=1000");
  const std::string request_path("Name");
  LookupContextPtr name_lookup_context = lookup(request_path);
  EXPECT_EQ(CacheEntryStatus::Unusable, lookup_result_.cache_entry_status_);

  Http::TestHeaderMapImpl response_headers{
    {"date", formatter_.fromTime(current_time_)},
    {"age", "6000"},
    {"cache-control", "public, max-age=9000"}};
  const std::string Body("Value");
  insert(move(name_lookup_context), response_headers, Body);
  EXPECT_TRUE(expectLookupSuccessWithBody(lookup(request_path).get(), Body));
  hz_cache_ptr->clearMaps();
}

TEST_F(HazelcastHttpCacheTest, ResponseStaleWithRequestLargeMaxStale) {
  request_headers_.setReferenceKey(Http::Headers::get().CacheControl,
      "max-stale=9000");

  const std::string request_path("Name");
  LookupContextPtr name_lookup_context = lookup(request_path);
  EXPECT_EQ(CacheEntryStatus::Unusable, lookup_result_.cache_entry_status_);

  Http::TestHeaderMapImpl response_headers{
    {"date", formatter_.fromTime(current_time_)},
    {"age", "7200"},
    {"cache-control", "public, max-age=3600"}};

  const std::string Body("Value");
  insert(move(name_lookup_context), response_headers, Body);
  EXPECT_TRUE(expectLookupSuccessWithBody(lookup(request_path).get(), Body));
  hz_cache_ptr->clearMaps();
}

TEST_F(HazelcastHttpCacheTest, StreamingPut) {

  Http::TestHeaderMapImpl response_headers{
    {"date", formatter_.fromTime(current_time_)},
    {"age", "2"},
    {"cache-control", "public, max-age=3600"}};
  InsertContextPtr inserter = hz_cache_ptr->
      makeInsertContext(lookup("request_path"));
  inserter->insertHeaders(response_headers, false);
  inserter->insertBody(
      Buffer::OwnedImpl("Hello, "),
      [](bool ready){EXPECT_TRUE(ready); }, false);
  inserter->insertBody(Buffer::OwnedImpl("World!"), nullptr, true);
  LookupContextPtr name_lookup_context = lookup("request_path");
  EXPECT_EQ(CacheEntryStatus::Ok, lookup_result_.cache_entry_status_);
  EXPECT_NE(nullptr, lookup_result_.headers_);
  ASSERT_EQ(13, lookup_result_.content_length_);
  EXPECT_EQ("Hello, World!", getBody(*name_lookup_context, 0, 13));
  hz_cache_ptr->clearMaps();
}

/*TEST(Registration, GetFactory) {
  envoy::extensions::filters::http::cache::v3::CacheConfig config;
  HttpCacheFactory* factory =
      Registry::FactoryRegistry<HttpCacheFactory>::
      getFactory("envoy.extensions.filters.http.cache.simple");
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->getCache(config)
    .cacheInfo().name_, "envoy.extensions.filters.http.cache.simple");
}*/

} // namespace
} // namespace Cache
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
