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

TEST(Publisher, Unix) {
  auto logger = spectator::DefaultLogger();
  const auto* dir = first_not_null(std::getenv("TMPDIR"), "/tmp");
  auto path = fmt::format("{}/testserver.{}", dir, getpid());
  TestUnixServer server{path};
  server.Start();
  logger->info("Unix Server started on path {}", path);
  SpectatordPublisher publisher{fmt::format("unix:{}", 0, path)};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto msgs = server.GetMessages();
  server.Stop();
  unlink(path.c_str());
  std::vector<std::string> expected{"c:counter:1", "c:counter:2"};
  EXPECT_EQ(server.GetMessages(), expected);
}

TEST(Publisher, Nop) {
  SpectatordPublisher publisher{"", 0};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
}

}  // namespace
