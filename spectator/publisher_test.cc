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
  SpectatordPublisher publisher{fmt::format("unix:{}", path), 10000, std::chrono::seconds(5)};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};

  // Wait for 3 seconds, increment, and the counter should not be flushed (3s is less than the 5s flush interval)
  std::this_thread::sleep_for(std::chrono::seconds(3));
  c.Increment();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  EXPECT_TRUE(msgs.empty());

  // Wait for another 3 seconds, increment, and the counter should be flushed (6s is greater than 5s flush interval)
  std::this_thread::sleep_for(std::chrono::seconds(3));
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

}  // namespace
