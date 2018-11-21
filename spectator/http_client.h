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
  HttpClient(Registry* registry, int connect_timeout, int read_timeout)
      : registry_(registry),
        connect_timeout_(connect_timeout),
        read_timeout_(read_timeout) {}

  int Post(const std::string& url, const char* content_type,
           const char* payload, size_t size) const;

  int Post(const std::string& url, const rapidjson::Document& payload) const;

  static void GlobalInit() noexcept;
  static void GlobalShutdown() noexcept;

 private:
  Registry* registry_;
  int connect_timeout_;
  int read_timeout_;

  int do_post(const std::string& url, std::unique_ptr<CurlHeaders> headers,
              std::unique_ptr<char[]> payload, size_t size) const;
};

}  // namespace spectator
