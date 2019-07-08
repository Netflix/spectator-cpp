#pragma once

#include <rapidjson/document.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace spectator {

class Registry;
class CurlHeaders;

class HttpClient {
 public:
  HttpClient(Registry* registry, std::chrono::milliseconds connect_timeout,
             std::chrono::milliseconds read_timeout)
      : registry_(registry),
        connect_timeout_(connect_timeout),
        total_timeout_(read_timeout + connect_timeout) {}

  int Post(const std::string& url, const char* content_type,
           const char* payload, size_t size, bool compress = true) const;

  int Post(const std::string& url, const rapidjson::Document& payload,
           bool compress = true) const;

  static void GlobalInit() noexcept;
  static void GlobalShutdown() noexcept;

 private:
  Registry* registry_;
  std::chrono::milliseconds connect_timeout_;
  std::chrono::milliseconds total_timeout_;

  int do_post(const std::string& url, std::shared_ptr<CurlHeaders> headers,
              std::shared_ptr<char> payload, size_t size,
              int attempt_number) const;
};

}  // namespace spectator
