#include <arpa/inet.h>
#include <atomic>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <zlib.h>

#include <fmt/ostream.h>
#include "../spectator/gzip.h"
#include "../spectator/http_client.h"
#include "../spectator/logger.h"
#include "../spectator/registry.h"
#include "../spectator/strings.h"
#include "../spectator/timer.h"
#include "http_server.h"
#include "test_utils.h"
#include "percentile_bucket_tags.inc"
#include "../spectator/percentile_buckets.h"

using spectator::Config;
using spectator::DefaultLogger;
using spectator::GetConfiguration;
using spectator::gzip_uncompress;
using spectator::HttpClient;
using spectator::Registry;
using spectator::Tags;

static std::shared_ptr<spectator::Meter> find_timer(
    Registry* registry, const std::string& name,
    const std::string& status_code) {
  auto meters = registry->Meters();
  for (const auto& m : meters) {
    if (m->GetType() == spectator::MeterType::Timer) {
      auto meter_name = m->MeterId()->Name();
      if (meter_name == name) {
        auto t = m->MeterId()->GetTags().at("http.status");
        if (t == status_code) {
          return m;
        }
      }
    }
  }
  return nullptr;
}

class TestRegistry : public Registry {
 public:
  TestRegistry(std::unique_ptr<Config> config)
      : Registry(std::move(config), DefaultLogger()) {}
};

TEST(HttpTest, Post) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, std::chrono::milliseconds(100),
                    std::chrono::milliseconds(100)};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  client.Post(url, "Content-type: application/json", post_data.c_str(),
              post_data.length());

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server.stop();

  auto timer_for_req = find_timer(&registry, "ipc.client.call", "200");
  ASSERT_TRUE(timer_for_req != nullptr);
  auto expected_tags =
      Tags{{"owner", "spectator-cpp"},   {"http.status", "200"},
           {"http.method", "POST"},      {"ipc.status", "success"},
           {"ipc.result", "success"},    {"ipc.attempt", "initial"},
           {"ipc.attempt.final", "true"}};

  const auto& actual_tags = timer_for_req->MeterId()->GetTags();
  EXPECT_EQ(expected_tags, actual_tags);

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), 1);

  const auto& r = requests[0];
  EXPECT_EQ(r.method(), "POST");
  EXPECT_EQ(r.path(), "/foo");
  EXPECT_EQ(r.get_header("Content-Encoding"), "gzip");
  EXPECT_EQ(r.get_header("Content-Type"), "application/json");

  const auto src = r.body();
  const auto src_len = r.size();
  char dest[8192];
  size_t dest_len = sizeof dest;
  auto res = gzip_uncompress(dest, &dest_len, src, src_len);
  ASSERT_EQ(res, Z_OK);

  std::string body_str{dest, dest_len};
  EXPECT_EQ(post_data, body_str);
}

TEST(HttpTest, Timeout) {
  using spectator::HttpClient;
  http_server server;
  server.set_read_sleep(std::chrono::milliseconds(100));
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  TestRegistry registry{GetConfiguration()};
  auto logger = registry.GetLogger();
  logger->info("Server started on port {}", port);

  HttpClient client{&registry, std::chrono::milliseconds(10),
                    std::chrono::milliseconds(10)};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto status = client.Post(url, "Content-type: application/json",
                            post_data.c_str(), post_data.length());
  server.stop();

  ASSERT_EQ(status, 400);
  auto timer_for_req = find_timer(&registry, "ipc.client.call", "-1");
  ASSERT_TRUE(timer_for_req != nullptr);

  auto expected_tags =
      Tags{{"owner", "spectator-cpp"}, {"http.status", "-1"},
           {"ipc.result", "failure"},  {"ipc.status", "timeout"},
           {"ipc.attempt", "initial"}, {"ipc.attempt.final", "true"},
           {"http.method", "POST"}};
  EXPECT_EQ(expected_tags, timer_for_req->MeterId()->GetTags());
}

TEST(HttpTest, ConnectTimeout) {
  using spectator::HttpClient;
  TestRegistry registry{GetConfiguration()};
  auto logger = registry.GetLogger();

  HttpClient client{&registry, std::chrono::milliseconds(10),
                    std::chrono::milliseconds(100)};
  const std::string url = "http://192.168.255.255:81/foo";
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto status = client.Post(url, "Content-type: application/json",
                            post_data.c_str(), post_data.length());

  ASSERT_EQ(status, 400);

  auto meters = registry.Meters();
  for (const auto& m : meters) {
    logger->info("{}", m->MeterId()->GetTags());
  }
}
