#include "publisher.h"
#include "logger.h"
#include <fmt/format.h>

namespace spectator {

SpectatordPublisher::SpectatordPublisher(std::string_view endpoint)
    : udp_socket_(io_context_), local_socket_(io_context_) {
  if (absl::StartsWith(endpoint, "unix:")) {
    setup_unix_domain(endpoint.substr(5));
  } else if (absl::StartsWith(endpoint, "udp:")) {
    auto pos = 4;
    // if the user used udp://foo:1234 instead of udp:foo:1234
    // adjust accordingly
    if (endpoint.substr(pos, 2) == "//") {
      pos += 2;
    }
    setup_udp(endpoint.substr(pos));
  } else if (endpoint != "disabled") {
    DefaultLogger()->warn(
        "Unknown endpoint: {}. Expecting: 'unix:/path/to/socket'"
        " or 'udp:hostname:port' - Will not send metrics",
        endpoint);
  }
}

void SpectatordPublisher::setup_unix_domain(std::string_view path) {
  using endpoint_t = asio::local::datagram_protocol::endpoint;
  local_socket_.connect(endpoint_t(std::string(path)));
  sender_ = [this](std::string_view msg) {
    local_socket_.send(asio::buffer(msg));
  };
}

void SpectatordPublisher::setup_udp(std::string_view host_port) {
  using asio::ip::udp;
  udp::resolver resolver{io_context_};

  auto end_host = host_port.find(':');
  if (end_host == std::string_view::npos) {
    auto err = fmt::format(
        "Unable to parse udp endpoint: '{}'. Expecting hostname:port",
        host_port);
    throw std::runtime_error(err);
  }

  auto host = host_port.substr(0, end_host);
  auto port = host_port.substr(end_host + 1);
  udp::endpoint endpoint = *resolver.resolve(udp::v6(), host, port);
  udp_socket_.connect(endpoint);
  sender_ = [this](std::string_view msg) {
    udp_socket_.send(asio::buffer(msg));
  };
}
}  // namespace spectator