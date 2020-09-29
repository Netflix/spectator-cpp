#include "publisher.h"
#include "counter.h"
#include "id.h"
#include "logger.h"
#include "test_server.h"
#include <gtest/gtest.h>
#include <unistd.h>

namespace {

using spectator::Counter;
using spectator::Id;
using spectator::Publisher;
using spectator::Tags;

TEST(Publisher, Udp) {
  TestUdpServer server;
  server.Start();
  auto logger = spectator::DefaultLogger();
  logger->info("Udp Server started on port {}", server.GetPort());

  Publisher publisher{fmt::format("udp:localhost:{}", server.GetPort())};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto msgs = server.GetMessages();
  server.Stop();
  std::vector<std::string> expected{"1:c:counter:1", "1:c:counter:2"};
  EXPECT_EQ(server.GetMessages(), expected);
}

const char* first_not_null(char* a, const char* b) {
  if (a != nullptr) return a;
  return b;
}

TEST(Publisher, Unix) {
  auto logger = spectator::DefaultLogger();
  auto dir = first_not_null(std::getenv("TMPDIR"), "/tmp");
  auto path = fmt::format("{}/testserver.{}", dir, getpid());
  TestUnixServer server{path};
  server.Start();
  logger->info("Unix Server started on path {}", path);
  Publisher publisher{fmt::format("unix:{}", path)};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto msgs = server.GetMessages();
  server.Stop();
  unlink(path.c_str());
  std::vector<std::string> expected{"1:c:counter:1", "1:c:counter:2"};
  EXPECT_EQ(server.GetMessages(), expected);
}

}  // namespace
