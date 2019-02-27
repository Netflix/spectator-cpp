#include "http_client.h"
#include "gzip.h"
#include "json.h"
#include "logger.h"
#include "memory.h"
#include "registry.h"
#include "strings.h"

#include <algorithm>
#include <curl/curl.h>
#include <curl/multi.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace spectator {

class CurlHeaders {
 public:
  CurlHeaders() = default;
  ~CurlHeaders() { curl_slist_free_all(list_); }
  CurlHeaders(const CurlHeaders&) = delete;
  CurlHeaders(CurlHeaders&&) = delete;
  CurlHeaders& operator=(const CurlHeaders&) = delete;
  CurlHeaders& operator=(CurlHeaders&&) = delete;
  void append(const std::string& string) {
    list_ = curl_slist_append(list_, string.c_str());
  }
  curl_slist* headers() { return list_; }

 private:
  curl_slist* list_{nullptr};
};

namespace {

constexpr const char* const kUserAgent = "spectator-cpp/1.0";
static size_t curl_ignore_output_fun(char*, size_t size, size_t nmemb, void*) {
  return size * nmemb;
}

class CurlHandle {
 public:
  CurlHandle() noexcept : handle_{curl_easy_init()} {
    curl_easy_setopt(handle_, CURLOPT_USERAGENT, kUserAgent);
  }

  CurlHandle(const CurlHandle&) = delete;

  CurlHandle& operator=(const CurlHandle&) = delete;

  CurlHandle(CurlHandle&& other) = delete;

  CurlHandle& operator=(CurlHandle&& other) = delete;

  ~CurlHandle() {
    // nullptr is handled by curl
    curl_easy_cleanup(handle_);
  }

  CURL* handle() const noexcept { return handle_; }

  CURLcode perform() { return curl_easy_perform(handle()); }

  CURLcode set_opt(CURLoption option, const void* param) {
    return curl_easy_setopt(handle(), option, param);
  }

  int status_code() {
    // curl requires this to be a long
    long http_code = 400;
    curl_easy_getinfo(handle(), CURLINFO_RESPONSE_CODE, &http_code);
    return static_cast<int>(http_code);
  }

  void set_url(const std::string& url) { set_opt(CURLOPT_URL, url.c_str()); }

  void set_headers(std::unique_ptr<CurlHeaders> headers) {
    headers_ = std::move(headers);
    set_opt(CURLOPT_HTTPHEADER, headers_->headers());
  }

  void set_connect_timeout(int connect_timeout_seconds) {
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT,
                     (long)connect_timeout_seconds);
  }

  void set_read_timeout(int read_timeout_seconds) {
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT, (long)read_timeout_seconds);
  }

  void post_payload(std::unique_ptr<char[]> payload, size_t size) {
    payload_ = std::move(payload);
    curl_easy_setopt(handle_, CURLOPT_POST, 1L);
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, payload_.get());
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, size);
  }

  void ignore_output() {
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, curl_ignore_output_fun);
  }

 private:
  CURL* handle_;
  std::unique_ptr<CurlHeaders> headers_;
  std::unique_ptr<char[]> payload_;
};

void add_status_tags(Tags* tags, CURLcode curl_res, int status_code) {
  if (curl_res == CURLE_OK) {
    auto code = fmt::format("{}", status_code);
    auto status = fmt::format("{}xx", status_code / 100);

    tags->add("status", status);
    tags->add("statusCode", code);
  } else if (curl_res == CURLE_OPERATION_TIMEDOUT) {
    tags->add("status", "timeout");
    tags->add("statusCode", "timeout");
  } else {
    tags->add("status", "error");
    tags->add("statusCode", "error");
  }
}

}  // namespace

int HttpClient::do_post(const std::string& url,
                        std::unique_ptr<CurlHeaders> headers,
                        std::unique_ptr<char[]> payload, size_t size) const {
  const auto start = Registry::clock::now();
  Tags tags{
      {"method", "POST"}, {"mode", "http-client"}, {"client", "spectator-cpp"}};
  CurlHandle curl;
  curl.set_connect_timeout(connect_timeout_);
  curl.set_read_timeout(read_timeout_);

  auto logger = registry_->GetLogger();
  logger->debug("POSTing to url: {}", url);
  curl.set_url(url);
  curl.set_headers(std::move(headers));
  curl.post_payload(std::move(payload), size);
  curl.ignore_output();

  auto curl_res = curl.perform();
  auto error = false;
  auto http_code = 400;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to POST {}: {}", url, curl_easy_strerror(curl_res));
    error = true;
    if (curl_res == CURLE_OPERATION_TIMEDOUT) {
      tags.add("status", "timeout");
      tags.add("statusCode", "timeout");
    } else {
      tags.add("status", "error");
      tags.add("statusCode", "error");
    }
  } else {
    http_code = curl.status_code();
    add_status_tags(&tags, curl_res, http_code);
  }

  if (!error) {
    logger->debug("Was able to POST to {} - status code: {}", url, http_code);
  }

  auto duration = Registry::clock::now() - start;
  registry_->GetTimer(registry_->CreateId("http.req.complete", tags))
      ->Record(duration);
  return http_code;
}

static constexpr const char* const kJsonType = "Content-Type: application/json";
static constexpr const char* const kGzipEncoding = "Content-Encoding: gzip";

int HttpClient::Post(const std::string& url, const char* content_type,
                     const char* payload, size_t size) const {
  auto headers = std::make_unique<CurlHeaders>();
  headers->append(content_type);
  headers->append(kGzipEncoding);
  auto compressed_size = compressBound(size) + kGzipHeaderSize;
  auto compressed_payload = std::unique_ptr<char[]>(new char[compressed_size]);
  auto compress_res =
      gzip_compress(compressed_payload.get(), &compressed_size, payload, size);
  if (compress_res != Z_OK) {
    registry_->GetLogger()->error(
        "Failed to compress payload: {}, while posting to {} - uncompressed "
        "size: {}",
        compress_res, url, size);
    return 400;
  }

  return do_post(url, std::move(headers), std::move(compressed_payload),
                 compressed_size);
}

int HttpClient::Post(const std::string& url,
                     const rapidjson::Document& payload) const {
  rapidjson::StringBuffer buffer;
  auto c_str = JsonGetString(buffer, payload);
  return Post(url, kJsonType, c_str, std::strlen(c_str));
}

void HttpClient::GlobalInit() noexcept { curl_global_init(CURL_GLOBAL_ALL); }

void HttpClient::GlobalShutdown() noexcept { curl_global_cleanup(); }

}  // namespace spectator
