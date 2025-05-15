#pragma once

#include "logger.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include <asio.hpp>

namespace spectator {

class SpectatordPublisher {
 public:
  explicit SpectatordPublisher(
      absl::string_view endpoint,
      uint32_t bytes_to_buffer = 0,
      std::shared_ptr<spdlog::logger> logger = DefaultLogger());
  SpectatordPublisher(const SpectatordPublisher&) = delete;

  ~SpectatordPublisher() {
      shutdown_.store(true);
      cv_receiver_.notify_all();
      cv_sender_.notify_all();
      if (sendingThread_.joinable()) {
        sendingThread_.join();
      }
  }

  void send(std::string_view measurement) { sender_(measurement); };

  void taskThreadFunction();
  bool try_to_send(const std::string& buffer);

 protected:
  using sender_fun = std::function<void(std::string_view)>;
  sender_fun sender_;

 private:
  void setup_nop_sender();
  void setup_unix_domain();
  void setup_udp(absl::string_view host_port);
  void local_reconnect(absl::string_view path);
  void udp_reconnect(const asio::ip::udp::endpoint& endpoint);

  std::shared_ptr<spdlog::logger> logger_;
  asio::io_context io_context_;
  asio::ip::udp::socket udp_socket_;
  asio::local::datagram_protocol::socket local_socket_;
  std::string buffer_;
  uint32_t bytes_to_buffer_;

  std::thread sendingThread_;
  std::mutex mtx_;
  std::condition_variable cv_receiver_;
  std::condition_variable cv_sender_;
  std::string unixDomainPath_;
  std::atomic<bool> shutdown_{false};
};

}  // namespace spectator
