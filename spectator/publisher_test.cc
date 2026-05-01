#include "id.h"
#include "logger.h"
#include "publisher.h"
#include "stateless_meters.h"
#include "test_server.h"
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

using spectator::Counter;
using spectator::Id;
using spectator::SpectatordPublisher;
using spectator::Tags;

TEST(Publisher, Udp) {
  // travis does not support udp on its container
  if (std::getenv("TRAVIS_COMPILER") == nullptr) {
    TestUdpServer server;
    server.Start();
    auto logger = spectator::DefaultLogger();
    logger->info("Udp Server started on port {}", server.GetPort());

    SpectatordPublisher publisher{
        fmt::format("udp:localhost:{}", server.GetPort()), 0};
    Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
    c.Increment();
    c.Add(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto msgs = server.GetMessages();
    server.Stop();
    std::vector<std::string> expected{"c:counter:1", "c:counter:2"};
    EXPECT_EQ(server.GetMessages(), expected);
  }
}

const char* first_not_null(char* a, const char* b) {
  if (a != nullptr) return a;
  return b;
}

TEST(Publisher, UnixNoBuffer) {
  auto logger = spectator::DefaultLogger();
  const auto* dir = first_not_null(std::getenv("TMPDIR"), "/tmp");
  auto path = fmt::format("{}/testserver.{}", dir, getpid());
  TestUnixServer server{path};
  server.Start();
  logger->info("Unix Server started on path {}", path);
  SpectatordPublisher publisher{fmt::format("unix:{}", path), 0};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  server.Stop();
  unlink(path.c_str());
  std::vector<std::string> expected{"c:counter:1", "c:counter:2"};
  EXPECT_EQ(msgs, expected);
}

TEST(Publisher, UnixBuffer) {
  auto logger = spectator::DefaultLogger();
  const auto* dir = first_not_null(std::getenv("TMPDIR"), "/tmp");
  auto path = fmt::format("{}/testserver.{}", dir, getpid());
  TestUnixServer server{path};
  server.Start();
  logger->info("Unix Server started on path {}", path);
  // Do not send until we buffer 32 bytes of data.
  SpectatordPublisher publisher{fmt::format("unix:{}", path), 32};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Increment();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  std::vector<std::string> emptyVector {};
  EXPECT_EQ(msgs, emptyVector);
  c.Increment();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  msgs = server.GetMessages();
  std::vector<std::string> expected{"c:counter:1\nc:counter:1\nc:counter:1"};
  EXPECT_EQ(msgs, expected);
  server.Stop();
  unlink(path.c_str());
}

TEST(Publisher, UnixBufferTimeFlush) {
  auto logger = spectator::DefaultLogger();
  const auto* dir = first_not_null(std::getenv("TMPDIR"), "/tmp");
  auto path = fmt::format("{}/testserver.{}", dir, getpid());
  TestUnixServer server{path};
  server.Start();
  logger->info("Unix Server started on path {}", path);

  // Set buffer size to a large value so that flushing is based on time
  SpectatordPublisher publisher{fmt::format("unix:{}", path), 10000, std::chrono::milliseconds(500)};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};

  // Wait for 300ms, increment, and the counter should not be flushed (300ms is less than the 500ms flush interval)
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  c.Increment();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  EXPECT_TRUE(msgs.empty());

  // Wait for another 300ms, increment, and the counter should be flushed (600ms is greater than 500ms flush interval)
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  c.Increment();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  msgs = server.GetMessages();
  std::vector<std::string> first_flush{"c:counter:1\nc:counter:1"};
  EXPECT_EQ(msgs, first_flush);

  server.Stop();
  unlink(path.c_str());
}

TEST(Publisher, Nop) {
  SpectatordPublisher publisher{"", 0};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
}

// Numeric IPv4 endpoint. Previously the resolver was pinned to udp::v6(),
// which crashed in IPv4-only environments (e.g. kind clusters); now the
// resolver runs as AF_UNSPEC and the socket family is taken from the
// resolved endpoint, so v4 hosts work regardless of host IPv6 support.
TEST(Publisher, UdpIpv4Numeric) {
  if (std::getenv("TRAVIS_COMPILER") != nullptr) return;
  TestUdpServer server;
  server.Start();
  auto logger = spectator::DefaultLogger();
  SpectatordPublisher publisher{
      fmt::format("udp:127.0.0.1:{}", server.GetPort()), 0};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  server.Stop();
  std::vector<std::string> expected{"c:counter:1", "c:counter:2"};
  EXPECT_EQ(msgs, expected);
}

// Bracketed IPv6 literal. Verifies split_host_port handles "[v6]:port" and
// the publisher opens a v6 socket against a v6 endpoint.
TEST(Publisher, UdpIpv6Bracketed) {
  if (std::getenv("TRAVIS_COMPILER") != nullptr) return;
  // Skip if IPv6 loopback isn't usable (some sandboxed envs disable it).
  asio::io_context io;
  asio::ip::udp::socket probe{io};
  std::error_code ec;
  probe.open(asio::ip::udp::v6(), ec);
  if (ec) {
    GTEST_SKIP() << "IPv6 not available: " << ec.message();
  }
  probe.close();

  TestUdpServer server;
  server.Start();
  SpectatordPublisher publisher{
      fmt::format("udp:[::1]:{}", server.GetPort()), 0};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  server.Stop();
  std::vector<std::string> expected{"c:counter:1"};
  EXPECT_EQ(msgs, expected);
}

// Regression: prior to soft-fail wrap + AF_UNSPEC resolver, an unresolvable
// hostname (or v4-only host with the old v6-pinned resolver) escaped the
// SpectatordPublisher constructor as asio::system_error and tripped
// std::terminate in embedders. Construction must not throw and subsequent
// emits must be silent no-ops.
TEST(Publisher, UdpUnresolvableHostFallsBackToNop) {
  // .invalid is RFC 2606 reserved — guaranteed not to resolve.
  ASSERT_NO_THROW({
    SpectatordPublisher publisher{"udp:nonexistent.invalid:1", 0};
    Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
    c.Increment();
    c.Add(2);
  });
}

// Malformed endpoint (no port) — must also be soft-fail, not throw.
TEST(Publisher, UdpMalformedEndpointFallsBackToNop) {
  ASSERT_NO_THROW({
    SpectatordPublisher publisher{"udp:no-port-here", 0};
    Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
    c.Increment();
  });
}

}  // namespace
