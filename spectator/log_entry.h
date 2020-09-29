#pragma once

#include "registry.h"
#include "strings.h"
#include "percentile_timer.h"

namespace spectator {
class LogEntry {
 public:
  LogEntry(Registry* registry, std::string method, const std::string& url)
      : registry_{registry},
        start_{absl::Now()},
        id_{registry_->CreateId("ipc.client.call",
                                Tags{{"owner", "spectator-cpp"},
                                     {"ipc.endpoint", PathFromUrl(url)},
                                     {"http.method", std::move(method)},
                                     {"http.status", "-1"}})} {}

  absl::Time start() const { return start_; }

  void log() {
    using millis = std::chrono::milliseconds;
    using std::chrono::seconds;
    registry_->GetPercentileTimer(id_, millis(1), seconds(5))
        ->Record(absl::Now() - start_);
  }

  void set_status_code(int code) {
    id_ = id_->WithTag("http.status", fmt::format("{}", code));
  }

  void set_attempt(int attempt_number, bool is_final) {
    id_ = id_->WithTag("ipc.attempt", attempt(attempt_number))
              ->WithTag("ipc.attempt.final", is_final ? "true" : "false");
  }

  void set_error(const std::string& error) {
    id_ = id_->WithTag("ipc.result", "failure")->WithTag("ipc.status", error);
  }

  void set_success() {
    const std::string ipc_success = "success";
    id_ = id_->WithTag("ipc.status", ipc_success)
              ->WithTag("ipc.result", ipc_success);
  }

 private:
  Registry* registry_;
  absl::Time start_;
  IdPtr id_;

  std::string attempt(int attempt_number) {
    static std::string initial = "initial";
    static std::string second = "second";
    static std::string third_up = "third_up";

    switch (attempt_number) {
      case 0:
        return initial;
      case 1:
        return second;
      default:
        return third_up;
    }
  }
};

}  // namespace spectator
