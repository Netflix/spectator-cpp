#include <utility>

#include "gzip.h"
#include "http_client.h"
#include "json.h"
#include "memory.h"
#include "percentile_timer.h"
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
size_t curl_ignore_output_fun(char* /*unused*/, size_t size, size_t nmemb,
                              void* /*unused*/) {
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

  void set_headers(std::shared_ptr<CurlHeaders> headers) {
    headers_ = std::move(headers);
    set_opt(CURLOPT_HTTPHEADER, headers_->headers());
  }

  void set_connect_timeout(std::chrono::milliseconds connect_timeout) {
    auto millis = static_cast<long>(connect_timeout.count());
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS, millis);
  }

  void set_timeout(std::chrono::milliseconds total_timeout) {
    auto millis = static_cast<long>(total_timeout.count());
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, millis);
  }

  void post_payload(std::shared_ptr<char> payload, size_t size) {
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
  std::shared_ptr<CurlHeaders> headers_;
  std::shared_ptr<char> payload_;
};

class LogEntry {
 public:
  LogEntry(Registry* registry, std::string method, std::string url)
      : registry_{registry},
        start_{Registry::clock::now()},
        id_{registry_->CreateId("ipc.client.call",
                                Tags{{"owner", "spectator-cpp"},
                                     {"ipc.endpoint", PathFromUrl(url)},
                                     {"http.method", std::move(method)},
                                     {"http.status", "-1"}})} {}

  Registry::clock::time_point start() const { return start_; }

  void log() {
    using millis = std::chrono::milliseconds;
    using std::chrono::seconds;
    PercentileTimer timer{registry_, std::move(id_), millis(1), seconds(5)};

    timer.Record(Registry::clock::now() - start_);
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
  Registry::clock::time_point start_;
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

}  // namespace

int HttpClient::do_post(const std::string& url,
                        std::shared_ptr<CurlHeaders> headers,
                        std::shared_ptr<char> payload, size_t size,
                        int attempt_number) const {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  LogEntry entry{registry_, "POST", url};

  CurlHandle curl;
  curl.set_timeout(total_timeout_);
  curl.set_connect_timeout(connect_timeout_);

  auto logger = registry_->GetLogger();
  logger->debug("POSTing to url: {}", url);
  curl.set_url(url);
  curl.set_headers(headers);
  curl.post_payload(payload, size);
  curl.ignore_output();

  auto curl_res = curl.perform();
  auto http_code = 400;

  if (curl_res != CURLE_OK) {
    logger->error("Failed to POST {}: {}", url, curl_easy_strerror(curl_res));
    switch (curl_res) {
      case CURLE_COULDNT_CONNECT:
        entry.set_error("connection_error");
        break;
      case CURLE_OPERATION_TIMEDOUT:
        entry.set_error("timeout");
        break;
      default:
        entry.set_error("unknown");
    }
    auto elapsed =
        duration_cast<milliseconds>(Registry::clock::now() - entry.start());
    // retry connect timeouts if possible, not read timeouts
    logger->info("HTTP timeout to {}: {}ms elapsed - connect_to={} read_to={}",
                 url, elapsed.count(), connect_timeout_.count(),
                 (total_timeout_ - connect_timeout_).count());
    if (elapsed < total_timeout_ && attempt_number < 2) {
      entry.set_attempt(attempt_number, false);
      entry.log();
      return do_post(url, std::move(headers), std::move(payload), size,
                     attempt_number + 1);
    }

    entry.set_status_code(-1);
  } else {
    http_code = curl.status_code();
    entry.set_status_code(http_code);
    if (http_code / 100 == 2) {
      entry.set_success();
    } else {
      entry.set_error("http_error");
    }
    logger->debug("Was able to POST to {} - status code: {}", url, http_code);
  }
  entry.set_attempt(attempt_number, true);
  entry.log();

  return http_code;
}

static constexpr const char* const kJsonType = "Content-Type: application/json";
static constexpr const char* const kGzipEncoding = "Content-Encoding: gzip";

int HttpClient::Post(const std::string& url, const char* content_type,
                     const char* payload, size_t size) const {
  auto headers = std::make_shared<CurlHeaders>();
  headers->append(content_type);
  headers->append(kGzipEncoding);
  auto compressed_size = compressBound(size) + kGzipHeaderSize;
  auto compressed_payload = std::shared_ptr<char>(
      new char[compressed_size], [](const char* p) { delete[] p; });
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
                 compressed_size, 0);
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
