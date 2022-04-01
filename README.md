[![Snapshot](https://github.com/Netflix/spectator-cpp/actions/workflows/snapshot.yml/badge.svg)](https://github.com/Netflix/spectator-cpp/actions/workflows/snapshot.yml) [![Release](https://github.com/Netflix/spectator-cpp/actions/workflows/release.yml/badge.svg)](https://github.com/Netflix/spectator-cpp/actions/workflows/release.yml)

# Spectator-cpp

> :warning: Experimental, Telemetry Team Only

This project implements a basic [Spectator] library for instrumenting C++ applications and sending
metrics to an Atlas Aggregator service.

**This library should only be used by the Telemetry Team** as a component for other projects, such
as [SpectatorD] or [Atlas System Agent]. Any uses of this library outside of these two projects
predate the release of the [SpectatorD] project and should **not** be used for reference purposes.

If you need to publish metrics from C++ projects, then you should use the [SpectatorD] service
directly, which is considered the primary metrics publishing interface.

[Spectator]: https://github.com/Netflix/spectator
[SpectatorD]: https://github.com/Netflix-Skunkworks/spectatord
[Atlas System Agent]: https://github.com/Netflix-Skunkworks/atlas-system-agent

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
  spectator::Registry registry{spectator::GetConfiguration()};

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

## Updating Dependencies

As an example, updating to the latest [spdlog](https://github.com/gabime/spdlog), which has a
dependency on [fmt](https://fmt.dev/latest/index.html):

* Download the latest fmt release. Unzip.

* Replace the fmt files in the 3rd-party tree.

      rm 3rd-party/fmt/*.h
      rm 3rd-party/fmt/*.cc
      cp $FMT_HOME/include/fmt/*.h 3rd-party/fmt
      cp $FMT_HOME/src/*.cc 3rd-party/fmt

* Commit with a message indicating the version.

* Clone the spdlog source. Check out the latest tagged release.

* Replace the spdlog files in the 3rd-party tree.

      rm -rf 3rd-party/spdlog/*
      cp -R $SPDLOG_HOME/include/spdlog/*  3rd-party/spdlog

* Commit with a message indicating the version.

## Branches

The `spectatord` branch is used by the [atlas-system-agent](https://github.com/Netflix-Skunkworks/atlas-system-agent)
project, because the main branch is used by a proxyd project. The difference between these two
branches is whether publishing is direct to the Atlas backends, or if it is sent to
[spectatord](https://github.com/Netflix-Skunkworks/spectatord). We will need to flip this around,
so that mainline development can proceed until the proxyd project can migrate to spectatord.
