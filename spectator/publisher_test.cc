#include "id.h"
#include "logger.h"
#include "publisher.h"
#include "stateless_meters.h"
#include "test_server.h"
#include <gtest/gtest.h>
#include <unistd.h>
#include <regex>

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
  std::vector<std::string> expected{"c:counter:1\nc:counter:1\nc:counter:1\n"};
  EXPECT_EQ(msgs, expected);
  server.Stop();
  unlink(path.c_str());
}

TEST(Publisher, Nop) {
  SpectatordPublisher publisher{"", 0};
  Counter c{std::make_shared<Id>("counter", Tags{}), &publisher};
  c.Increment();
  c.Add(2);
}

TEST(Publisher, MultiThreadedCounters) {
  auto logger = spectator::DefaultLogger();
  const auto* dir = first_not_null(std::getenv("TMPDIR"), "/tmp");
  auto path = fmt::format("{}/testserver.{}", dir, getpid());
  TestUnixServer server{path};
  server.Start();
  logger->info("Unix Server started on path {}", path);
 
  // Create publisher with a small buffer size to ensure flushing
  SpectatordPublisher publisher{fmt::format("unix:{}", path), 50};
 
  // Number of threads and counters to create
  const int numThreads = 4;
  const int countersPerThread = 3;
  const int incrementsPerCounter = 5;
 
  // Function for worker threads
  auto worker = [&](int threadId) {
    // Create several counters per thread with unique names
    for (int i = 0; i < countersPerThread; i++) {
      std::string counterName = fmt::format("counter.thread{}.{}", threadId, i);
      Counter counter(std::make_shared<Id>(counterName, Tags{}), &publisher);
     
      // Increment each counter multiple times
      for (int j = 0; j < incrementsPerCounter; j++) {
        counter.Increment();
      }
    }
  };
 
  // Start worker threads
  std::vector<std::thread> threads;
  for (int i = 0; i < numThreads; i++) {
    threads.emplace_back(worker, i);
  }
 
  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }
 
  // Give some time for messages to be sent
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
 
  // Check messages
  auto msgs = server.GetMessages();
  EXPECT_FALSE(msgs.empty());
 
  // Verify total number of increments
  int expectedIncrements = numThreads * countersPerThread * incrementsPerCounter;
  int actualIncrements = 0;
 
  // Verify every string in msgs follows the form counter.thread<digit>.<digit>
  std::regex counter_regex(R"(c:counter\.thread\d+\.\d+:1)");
  for (const auto& msg : msgs) {
    std::stringstream ss(msg);
    std::string line;
    while (std::getline(ss, line)) {
      if (!line.empty()) {
        EXPECT_TRUE(std::regex_match(line, counter_regex))
            << "Unexpected counter format: " << line;
        actualIncrements++;
      }
    }
  }
 
  EXPECT_EQ(actualIncrements, expectedIncrements);
 
  server.Stop();
  unlink(path.c_str());
}

}  // namespace
