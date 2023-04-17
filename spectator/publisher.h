#pragma once

#include "logger.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include <asio.hpp>

namespace spectator {

static const uint32_t MAX_BUFFER_SIZE = 16 * 1024;

class SpectatordPublisher {
 public:
  explicit SpectatordPublisher(
      absl::string_view endpoint,
      uint32_t maxBufferSize = MAX_BUFFER_SIZE,
      std::shared_ptr<spdlog::logger> logger = DefaultLogger());
  SpectatordPublisher(const SpectatordPublisher&) = delete;

  void send(std::string_view measurement) { sender_(measurement); };

 protected:
  using sender_fun = std::function<void(std::string_view)>;
  sender_fun sender_;

 private:
  void setup_nop_sender();
  void setup_unix_domain(absl::string_view path);
  void setup_udp(absl::string_view host_port);
  void local_reconnect(absl::string_view path);
  void udp_reconnect(const asio::ip::udp::endpoint& endpoint);

  std::shared_ptr<spdlog::logger> logger_;
  asio::io_context io_context_;
  asio::ip::udp::socket udp_socket_;
  asio::local::datagram_protocol::socket local_socket_;
  std::string buffer_;
  uint32_t max_buffer_size_;
};

}  // namespace spectator
