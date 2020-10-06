#pragma once

#include "config.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include <asio.hpp>

namespace spectator {

class SpectatordPublisher {
 public:
  explicit SpectatordPublisher(std::string_view endpoint);
  SpectatordPublisher(const SpectatordPublisher&) = delete;

  void send(std::string_view measurement) { sender_(measurement); };

 protected:
  using sender_fun = std::function<void(std::string_view)>;
  sender_fun sender_;

 private:
  void setup_unix_domain(std::string_view path);
  void setup_udp(std::string_view host_port);

  Config config_;
  asio::io_context io_context_;
  asio::ip::udp::socket udp_socket_;
  asio::local::datagram_protocol::socket local_socket_;
};

}  // namespace spectator
