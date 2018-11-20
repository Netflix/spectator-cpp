[![Build Status](https://travis-ci.org/Netflix/spectator-cpp.svg?branch=master)](https://travis-ci.org/Netflix/spectator-cpp)
[![codecov](https://codecov.io/gh/Netflix/spectator-cpp/branch/master/graph/badge.svg)](https://codecov.io/gh/Netflix/spectator-cpp)

# Spectator-cpp

> :warning: Experimental

Simple library for instructing code to record dimentional time series.

## Description

This implements a basic [Spectator](https://github.com/Netflix/spectator) library
for instrumenting C++ applications, sending metrics to an Atlas aggregator service.

## Instrumenting Code

```C++
#include <spectator/registry.h>

// use default values
static constexpr auto kDefault = 0;

struct Request {
  std::string country;
};

struct Response {
  int status;
  int size;
};

class Server {
 public:
  explicit Server(spectator::Registry* registry)
      : registry_{registry},
        request_count_id_{registry->CreateId("server.requestCount", spectator::Tags{})},
        request_latency_{registry->GetTimer("server.requestLatency")},
        response_size_{registry->GetDistributionSummary("server.responseSizes")} {}

  Response Handle(const Request& request) {
    using spectator::Registry;
    auto start = Registry::clock::now();

    // do some work and obtain a response...
    Response res{200, 64};

    // Update the counter id with dimensions based on the request. The
    // counter will then be looked up in the registry which should be
    // fairly cheap, such as lookup of id object in a map
    // However, it is more expensive than having a local variable set
    // to the counter.
    auto cnt_id = request_count_id_->WithTag("country", request.country)
                      ->WithTag("status", std::to_string(res.status));
    registry_->GetCounter(std::move(cnt_id))->Increment();
    request_latency_->Record(Registry::clock::now() - start);
    response_size_->Record(res.size);
    return res;
  }

 private:
  spectator::Registry* registry_;
  std::shared_ptr<spectator::Id> request_count_id_;
  std::shared_ptr<spectator::Timer> request_latency_;
  std::shared_ptr<spectator::DistributionSummary> response_size_;
};

Request get_next_request() {
  //...
  return Request{"US"};
}

int main() {
  std::map<std::string, std::string> common_tags = {{"nf.app", "example"},
                                                    {"nf.region", "us-west-1"}};
  spectator::Config config{common_tags, kDefault, kDefault,
                           kDefault,    kDefault, "http://example.org/api/v1/publish"};
  spectator::Registry registry{config};

  registry.Start();

  Server server{&registry};

  for (auto i = 1; i <= 3; ++i) {
    // get a request
    auto req = get_next_request();
    server.Handle(req);
  }

  registry.Stop();
}
```
