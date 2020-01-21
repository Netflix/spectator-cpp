#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <unordered_map>
#include "memory.h"

namespace spectator {

class Registry;
class CurlHeaders;

struct HttpClientConfig {
  std::chrono::milliseconds connect_timeout;
  std::chrono::milliseconds read_timeout;
  bool compress;
  bool read_headers;
  bool read_body;
};

using HttpHeaders = std::unordered_map<std::string, std::string>;
struct HttpResponse {
  int status;
  std::string raw_body;
  HttpHeaders headers;
};

class HttpClient {
 public:
  static constexpr const char* const kJsonType =
      "Content-Type: application/json";

  HttpClient(Registry* registry, HttpClientConfig config);

  HttpResponse Post(const std::string& url, const char* content_type,
                    const char* payload, size_t size) const;

  HttpResponse Post(const std::string& url, const char* content_type,
                    const std::string& payload) const {
    return Post(url, content_type, payload.c_str(), payload.length());
  };

  HttpResponse Post(const std::string& url,
                    const rapidjson::Document& payload) const;

  HttpResponse Get(const std::string& url) const;
  HttpResponse Get(const std::string& url,
                   const std::vector<std::string>& headers) const;

  static void GlobalInit() noexcept;
  static void GlobalShutdown() noexcept;

 private:
  Registry* registry_;
  HttpClientConfig config_;

  HttpResponse perform(const char* method, const std::string& url,
                       std::shared_ptr<CurlHeaders> headers,
                       const char* payload, size_t size,
                       int attempt_number) const;

  std::string payload_to_str(const rapidjson::Document& payload) const;
};

}  // namespace spectator
