#include "publisher.h"
#include "logger.h"
#include <fmt/format.h>

namespace spectator {

static const char NEW_LINE = '\n';

SpectatordPublisher::SpectatordPublisher(absl::string_view endpoint,
                                         uint32_t bytes_to_buffer,
                                         std::chrono::milliseconds flush_interval,
                                         std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger)),
      udp_socket_(io_context_),
      local_socket_(io_context_),
      bytes_to_buffer_(bytes_to_buffer),
      last_flush_time_(std::chrono::steady_clock::now()),
      flush_interval_(flush_interval) {
  buffer_.reserve(bytes_to_buffer_ + 1024);     
  if (absl::StartsWith(endpoint, "unix:")) {
    setup_unix_domain(endpoint.substr(5));
  } else if (absl::StartsWith(endpoint, "udp:")) {
    auto pos = 4;
    // if the user used udp://foo:1234 instead of udp:foo:1234
    // adjust accordingly
    if (endpoint.substr(pos, 2) == "//") {
      pos += 2;
    }
    // resolve_host_port (called from setup_udp) throws asio::system_error
    // when the resolver fails — e.g. for an unresolvable hostname or transient
    // DNS outage. An exception escaping this constructor unwinds through the
    // embedder's stats-init path and can trigger std::terminate. Fall back to
    // the nop sender on any setup failure so callers stay alive and just lose
    // metrics, mirroring the soft-fail behavior of local_reconnect /
    // udp_reconnect for post-construction errors.
    try {
      setup_udp(endpoint.substr(pos));
    } catch (const std::exception& e) {
      logger_->warn(
          "Unable to setup udp publisher for '{}': {} - Will not send metrics",
          std::string(endpoint), e.what());
      setup_nop_sender();
    }
  } else if (endpoint != "disabled") {
    logger_->warn(
        "Unknown endpoint: '{}'. Expecting: 'unix:/path/to/socket'"
        " or 'udp:hostname:port' - Will not send metrics",
        std::string(endpoint));
    setup_nop_sender();
  }
}

void SpectatordPublisher::setup_nop_sender() {
  sender_ = [this](std::string_view msg) { logger_->trace("{}", msg); };
}

void SpectatordPublisher::local_reconnect(absl::string_view path) {
  using endpoint_t = asio::local::datagram_protocol::endpoint;
  try {
    if (local_socket_.is_open()) {
      local_socket_.close();
    }
    local_socket_.open();
    local_socket_.connect(endpoint_t(std::string(path)));
  } catch (std::exception& e) {
    logger_->warn("Unable to connect to {}: {}", std::string(path), e.what());
  }
}

void SpectatordPublisher::setup_unix_domain(absl::string_view path) {
  local_reconnect(path);
  // get a copy of the file path
  std::string local_path{path};

  flusher_ = [local_path, this]() {
    if (buffer_.empty()) return;
    const auto now = std::chrono::steady_clock::now();
    for (auto i = 0; i < 3; ++i) {
      try {
        auto sent_bytes = local_socket_.send(asio::buffer(buffer_));
        logger_->trace("Sent (local): {} bytes, in total had {}", sent_bytes, buffer_.length());
        last_flush_time_ = now;
        break;
      } catch (std::exception& e) {
        local_reconnect(local_path);
        logger_->warn("Unable to send {} - attempt {}/3 ({})", buffer_, i,
                      e.what());
      }
    }
    buffer_.clear();
  };

  sender_ = [this](std::string_view msg) {
    buffer_.append(msg);
    const auto now = std::chrono::steady_clock::now();
    const bool should_flush = buffer_.length() >= bytes_to_buffer_ ||
                        now - last_flush_time_ >= flush_interval_;

    if (should_flush) {
      flusher_();
    } else {
      buffer_.push_back(NEW_LINE);
    }
  };
}

// Splits "host:port" or bracketed "[v6-host]:port" into (host, port).
// Bracketed form is required for IPv6 literals so the multiple ':' in the
// address are not confused with the host/port separator.
inline std::pair<std::string, std::string> split_host_port(
    absl::string_view host_port) {
  if (!host_port.empty() && host_port.front() == '[') {
    auto close = host_port.find(']');
    if (close == absl::string_view::npos || close + 1 >= host_port.size() ||
        host_port[close + 1] != ':') {
      throw std::runtime_error(fmt::format(
          "Unable to parse udp endpoint: '{}'. Expecting [ipv6-host]:port",
          std::string(host_port)));
    }
    return {std::string(host_port.substr(1, close - 1)),
            std::string(host_port.substr(close + 2))};
  }
  // Use rfind so a bare (unbracketed) IPv6 literal at least lands on the last
  // ':' rather than the first; bracketed form is still the supported way.
  auto sep = host_port.rfind(':');
  if (sep == absl::string_view::npos) {
    throw std::runtime_error(fmt::format(
        "Unable to parse udp endpoint: '{}'. Expecting hostname:port",
        std::string(host_port)));
  }
  return {std::string(host_port.substr(0, sep)),
          std::string(host_port.substr(sep + 1))};
}

inline asio::ip::udp::endpoint resolve_host_port(
    asio::io_context& io_context,  // NOLINT
    absl::string_view host_port) {
  using asio::ip::udp;
  udp::resolver resolver{io_context};

  auto [host, port] = split_host_port(host_port);
  // Resolve with AF_UNSPEC (no protocol pin) so we accept whatever family the
  // host actually supports — IPv4-only, IPv6-only, or dual-stack. The earlier
  // udp::v6() pin caused getaddrinfo to fail with EAI_ADDRFAMILY in IPv4-only
  // environments (kind clusters with net.ipv6.conf.all.disable_ipv6=1, etc.),
  // throwing out of the SpectatordPublisher constructor and crashing
  // embedders. asio orders the result set per RFC 6724, so on dual-stack
  // hosts the OS-preferred family wins.
  return *resolver.resolve(std::string(host), std::string(port));
}

void SpectatordPublisher::udp_reconnect(
    const asio::ip::udp::endpoint& endpoint) {
  try {
    if (udp_socket_.is_open()) {
      udp_socket_.close();
    }
    // Match the socket's protocol family to the resolved endpoint so we
    // open an AF_INET socket for v4 endpoints and AF_INET6 for v6 ones —
    // works on v4-only, v6-only, and dual-stack hosts.
    udp_socket_.open(endpoint.protocol());
    udp_socket_.connect(endpoint);
  } catch (std::exception& e) {
    logger_->warn("Unable to connect to {}: {}", endpoint.address().to_string(),
                  endpoint.port());
  }
}

void SpectatordPublisher::setup_udp(absl::string_view host_port) {
  auto endpoint = resolve_host_port(io_context_, host_port);
  udp_reconnect(endpoint);
  sender_ = [endpoint, this](std::string_view msg) {
    for (auto i = 0; i < 3; ++i) {
      try {
        udp_socket_.send(asio::buffer(msg));
        logger_->trace("Sent (udp): {}", msg);
        break;
      } catch (std::exception& e) {
        logger_->warn("Unable to send {} - attempt {}/3", msg, i);
        udp_reconnect(endpoint);
      }
    }
  };
}
}  // namespace spectator
