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

  const auto& logger = Logger();
  logger->info("POSTing to url: {}", url);
  curl.set_url(url);
  curl.set_headers(std::move(headers));
  curl.post_payload(std::move(payload), size);

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
    logger->info("Was able to POST to {} - status code: {}", url, http_code);
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
    Logger()->error(
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

class CurlMultiHandle {
 public:
  CurlMultiHandle() : multi_handle_(curl_multi_init()) {}
  CurlMultiHandle(const CurlMultiHandle&) = delete;
  CurlMultiHandle(CurlMultiHandle&&) = delete;
  CurlMultiHandle& operator=(const CurlMultiHandle&) = delete;
  CurlMultiHandle& operator=(CurlMultiHandle&&) = delete;
  ~CurlMultiHandle() { curl_multi_cleanup(multi_handle_); }

  std::vector<int> perform_all(
      const std::vector<std::unique_ptr<CurlHandle>>& easy_handles) {
    for (const auto& h : easy_handles) {
      curl_multi_add_handle(multi_handle_, h->handle());
    }

    auto still_running = 0;
    auto repeats = 0;
    do {
      auto mc = curl_multi_perform(multi_handle_, &still_running);
      auto numfds = 0;
      if (mc == CURLM_OK) {
        // wait for activity, timeout, or "nothing"
        mc = curl_multi_wait(multi_handle_, nullptr, 0, 1000, &numfds);
      }
      if (mc != CURLM_OK) {
        Logger()->warn("curl_multi failed: {}", curl_multi_strerror(mc));
        break;
      }

      // 'numfds' being zero means either a timeout or no file descriptors to
      // wait for. Try timeout on first occurrence, then assume no file
      // descriptors and no file descriptors to wait for means wait for 100
      // milliseconds.
      if (numfds == 0) {
        repeats++;  // count number of repeated zero numfds
        if (repeats > 1) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      } else {
        repeats = 0;
      }
    } while (still_running > 0);

    CURLMsg* msg; /* for picking up messages with the transfer status */
    int msgs_left;
    std::vector<int> res(easy_handles.size());
    std::fill(res.begin(), res.end(), 400);
    while ((msg = curl_multi_info_read(multi_handle_, &msgs_left)) != nullptr) {
      if (msg->msg == CURLMSG_DONE) {
        // Find out which handle this message is about
        for (auto i = 0u; i < easy_handles.size(); i++) {
          auto easy_handle = easy_handles[i]->handle();
          bool found = (msg->easy_handle == easy_handle);
          if (found) {
            auto easy_code = msg->data.result;
            if (easy_code == CURLE_OK) {
              res.at(i) = easy_handles[i]->status_code();
            } else {
              Logger()->warn("Failed batch: {}", curl_easy_strerror(easy_code));
              res.at(i) = 400;
            }
            curl_multi_remove_handle(multi_handle_, easy_handle);
            break;
          }
        }
      }
    }
    return res;
  }

 private:
  CURLM* multi_handle_;
};

static bool setup_handle_for_post(CurlHandle* handle, const std::string& url,
                                  const rapidjson::Document& doc) {
  rapidjson::StringBuffer buffer;
  auto payload = JsonGetString(buffer, doc);
  auto size = std::strlen(payload);
  auto headers = std::make_unique<CurlHeaders>();
  headers->append(kJsonType);
  headers->append(kGzipEncoding);

  auto compressed_size = compressBound(size) + kGzipHeaderSize;
  auto compressed_payload = std::unique_ptr<char[]>(new char[compressed_size]);
  auto compress_res =
      gzip_compress(compressed_payload.get(), &compressed_size, payload, size);
  if (compress_res != Z_OK) {
    Logger()->error(
        "Failed to compress payload: {}, while posting to {} - uncompressed "
        "size: {}",
        compress_res, url, size);
    return false;
  }

  handle->set_url(url);
  handle->set_headers(std::move(headers));
  handle->post_payload(std::move(compressed_payload), size);
  return true;
}

std::vector<int> HttpClient::PostBatches(
    const std::string& url,
    const std::vector<rapidjson::Document>& batches) const {
  CurlMultiHandle multi_handle;

  std::vector<std::unique_ptr<CurlHandle>> easy_handles;
  auto start = Registry::clock::now();
  for (const auto& doc : batches) {
    auto handle = std::make_unique<CurlHandle>();
    handle->set_read_timeout(read_timeout_);
    handle->set_connect_timeout(connect_timeout_);
    setup_handle_for_post(handle.get(), url, doc);
    easy_handles.push_back(std::move(handle));
  }

  auto results = multi_handle.perform_all(easy_handles);
  auto duration = Registry::clock::now() - start;
  Tags tags{
      {"method", "POST"}, {"mode", "http-client"}, {"client", "spectator-cpp"}};
  for (const auto& result : results) {
    add_status_tags(&tags, CURLE_OK, result);
    registry_->GetTimer(registry_->CreateId("http.req.complete", tags))
        ->Record(duration);
  }
  return results;
}

void HttpClient::GlobalInit() noexcept { curl_global_init(CURL_GLOBAL_ALL); }

void HttpClient::GlobalShutdown() noexcept { curl_global_cleanup(); }

}  // namespace spectator
