#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <rapidjson/document.h>
#include "memory.h"

namespace spectator {

class Registry;
class CurlHeaders;

struct HttpClientConfig {
  std::chrono::milliseconds connect_timeout;
  std::chrono::milliseconds read_timeout;
  bool compress;
  size_t json_buffer_size;
};

class HttpClient {
 public:
  static constexpr const char* const kJsonType =
      "Content-Type: application/json";

  HttpClient(Registry* registry, HttpClientConfig config)
      : registry_(registry),
        config_{config},
        json_buffer_{
            std::unique_ptr<char[]>(new char[config.json_buffer_size])} {}

  int Post(const std::string& url, const char* content_type,
           const char* payload, size_t size) const;

  int Post(const std::string& url, const char* content_type,
           const std::string& payload) const {
    return Post(url, content_type, payload.c_str(), payload.length());
  };

  int Post(const std::string& url, const rapidjson::Document& payload) const;

  static void GlobalInit() noexcept;
  static void GlobalShutdown() noexcept;

 private:
  Registry* registry_;
  HttpClientConfig config_;
  std::unique_ptr<char[]> json_buffer_;

  int do_post(const std::string& url, std::shared_ptr<CurlHeaders> headers,
              const char* payload, size_t size, int attempt_number) const;

  std::string payload_to_str(const rapidjson::Document& payload) const;
};

}  // namespace spectator
