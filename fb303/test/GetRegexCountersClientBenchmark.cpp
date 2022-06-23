/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boost/regex.hpp>
#include <fb303/BaseService.h>
#include <fb303/test/gen-cpp2/TestService.h>
#include <folly/Benchmark.h>
#include <folly/DynamicConverter.h>
#include <folly/Optional.h>
#include <folly/Random.h>
#include <folly/String.h>
#include <folly/init/Init.h>
#include <thrift/lib/cpp2/util/ScopedServerInterfaceThread.h>
#include <chrono>
#include <functional>

#include <time.h>
using namespace std;
using namespace facebook::fb303;
using namespace folly;
std::unique_ptr<facebook::fb303::TestServiceAsyncClient> fb303Client;
apache::thrift::RpcOptions opt;
/* This test case creates a fb303 server and async client
 * The server has regex key caching enabled.
 * Hence it is able to look at previous patterns and send
 *  the counters whose keys match the pattern
 */
class TestHandler : public TestServiceSvIf, public BaseService {
 public:
  static const int kGetRegexCountersCalls = 100;

  TestHandler() : BaseService("TestService") {
    DynamicCounters* dynamicCounters = fbData->getDynamicCounters();
    const int kMaxIter = 3000;
    for (int iter = 0; iter < kMaxIter; iter++) {
      auto counterName =
          "matchingCounter" + folly::convertTo<std::string>(iter);
      dynamicCounters->registerCallback(counterName, [] { return 1; });
    }
    for (int iter = 0; iter < 2 * kMaxIter; iter++) {
      auto counterName = "counter" + folly::convertTo<std::string>(iter);
      dynamicCounters->registerCallback(counterName, [] { return 0; });
    }
  }

  cpp2::fb303_status getStatus() override {
    return cpp2::fb303_status::ALIVE;
  }
};

/* It calls getRegexCounter first call triggers caching on the server
 * Subsequent calls (for kGetRegexCountersIter-1) are optimized
 */

BENCHMARK(GetRegexCountersServerSideSubset) {
  for (int iter = 0; iter < TestHandler::kGetRegexCountersCalls; iter++) {
    std::map<std::string, int64_t> counters;
    fb303Client->sync_getRegexCounters(opt, counters, "matching.*");
  }
}

// match only one counter
BENCHMARK(GetRegexCountersServerSideOne) {
  // subsequent calls leverage the cache
  for (int iter = 0; iter < TestHandler::kGetRegexCountersCalls; iter++) {
    std::map<std::string, int64_t> counters;
    fb303Client->sync_getRegexCounters(opt, counters, "matchingCounter1");
  }
}

// matches all the counters with .*
BENCHMARK(GetRegexCountersServerSideAll) {
  // subsequent calls leverage the cache
  for (int iter = 0; iter < TestHandler::kGetRegexCountersCalls; iter++) {
    std::map<std::string, int64_t> counters;
    fb303Client->sync_getRegexCounters(opt, counters, ".*");
  }
}

// Matches a subset of counters
// gets all counters and applies filtering on client side
// This adds extra cpu utilization for serialization and deserialization
BENCHMARK(GetCountersClientSideFilteringSubset) {
  for (int iter = 0; iter < TestHandler::kGetRegexCountersCalls; iter++) {
    std::map<std::string, int64_t> counters;
    fb303Client->sync_getCounters(opt, counters);
    const boost::regex regexObject("matching.*");
    std::erase_if(counters, [&](const auto& item) {
      if (regex_match(item.first, regexObject)) {
        return true;
      }
      return false;
    });
  }
}

// matches only one counter
// gets all counters and applies filtering on client side
// This adds extra cpu utilization for serialization and deserialization
BENCHMARK(GetCountersClientSideFilteringOne) {
  for (int iter = 0; iter < TestHandler::kGetRegexCountersCalls; iter++) {
    std::map<std::string, int64_t> counters;
    fb303Client->sync_getCounters(opt, counters);
    const boost::regex regexObject("matchingCounter1");
    std::erase_if(counters, [&](const auto& item) {
      if (regex_match(item.first, regexObject)) {
        return true;
      }
      return false;
    });
  }
}

// Matches all counters with .*
BENCHMARK(GetCountersClientSideFilteringAll) {
  for (int iter = 0; iter < TestHandler::kGetRegexCountersCalls; iter++) {
    std::map<std::string, int64_t> counters;
    const boost::regex regexObject(".*");
    fb303Client->sync_getCounters(opt, counters);
    std::erase_if(counters, [&](const auto& item) {
      if (regex_match(item.first, regexObject)) {
        return true;
      }
      return false;
    });
  }
}

int main(int argc, char** argv) {
  folly::Init init{&argc, &argv, true};
  std::shared_ptr<TestHandler> handler = std::make_shared<TestHandler>();
  apache::thrift::ScopedServerInterfaceThread server(
      handler); // set up fb303 server
  auto const address = server.getAddress();
  opt = apache::thrift::RpcOptions();
  opt.setTimeout(std::chrono::seconds(10)); // timeout for thrift response
  fb303Client =
      server.newClient<facebook::fb303::TestServiceAsyncClient>(); // set up
                                                                   // client
  runBenchmarks();
  return 0;
}

/*
============================================================================
[...]t/GetRegexCountersClientBenchmark.cpp     relative  time/iter   iters/s
============================================================================
GetRegexCountersServerSideSubset                          600.58ms      1.67
GetRegexCountersServerSideOne                             181.84ms      5.50
GetRegexCountersServerSideAll                                1.78s   562.04m
GetCountersClientSideFilteringSubset                         1.12s   890.12m
GetCountersClientSideFilteringOne                         988.20ms      1.01
GetCountersClientSideFilteringAll                            1.40s   715.96m
*/