#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class http_server {
 public:
  http_server() noexcept;
  http_server(const http_server&) = delete;
  http_server(http_server&&) = delete;
  http_server& operator=(const http_server&) = delete;
  http_server& operator=(http_server&&) = delete;

  ~http_server();

  void set_read_sleep(int millis) { read_sleep_ = millis; }

  void set_accept_sleep(int millis) { accept_sleep_ = millis; }

  void start() noexcept;

  int get_port() const { return port_; };

  void stop() { is_done = true; }

  class Request {
   public:
    Request() : size_(0), body_(nullptr) {}
    Request(std::string method, std::string path,
            std::map<std::string, std::string> headers, size_t size,
            std::unique_ptr<char[]>&& body)
        : method_(std::move(method)),
          path_(std::move(path)),
          headers_(std::move(headers)),
          size_(size),
          body_(std::move(body)) {}

    size_t size() const { return size_; }
    const char* body() const { return body_.get(); }
    std::string get_header(const std::string& name) const;
    std::string method() const { return method_; }
    std::string path() const { return path_; }

   private:
    std::string method_;
    std::string path_;
    std::map<std::string, std::string> headers_{};
    size_t size_;
    std::unique_ptr<char[]> body_{};
  };

  const std::vector<Request>& get_requests() const;

 private:
  volatile int sockfd_ = -1;
  volatile int port_ = 0;
  volatile int accept_sleep_ = 0;
  volatile int read_sleep_ = 0;

  volatile bool is_done{false};
  mutable std::mutex requests_mutex_{};
  std::vector<Request> requests_;

  std::map<std::string, std::string> path_response_;

  void accept_request(int client);
  void accept_loop();
};
