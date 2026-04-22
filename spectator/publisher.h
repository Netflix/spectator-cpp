#pragma once

#include "logger.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include <asio.hpp>

namespace spectator {

// Thread safety: not thread-safe. All calls to send() and flush() must come
// from the same thread. The unix-domain buffered path shares mutable buffer_
// and last_flush_time_ without locking; the UDP path is incidentally safe
// (one atomic datagram per send) but that isn't a supported guarantee.
//
// This single-threaded contract matches the proxyd/Envoy use case, where
// meter sends and the flush() timer tick both run on the same dispatcher
// thread. Other callers must serialize externally.
class SpectatordPublisher {
 public:
  explicit SpectatordPublisher(
      absl::string_view endpoint,
      uint32_t bytes_to_buffer = 0,
      std::chrono::milliseconds flush_interval = std::chrono::milliseconds(60000),
      std::shared_ptr<spdlog::logger> logger = DefaultLogger());
  SpectatordPublisher(const SpectatordPublisher&) = delete;

  void send(std::string_view measurement) { sender_(measurement); };
  void flush() { flusher_(); };

 protected:
  using sender_fun = std::function<void(std::string_view)>;
  sender_fun sender_;
  using flusher_fun = std::function<void()>;
  flusher_fun flusher_ = []() {};

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
  uint32_t bytes_to_buffer_;
  std::chrono::steady_clock::time_point last_flush_time_;
  const std::chrono::milliseconds flush_interval_;
};

}  // namespace spectator
