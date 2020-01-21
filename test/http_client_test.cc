#include <atomic>
#include <gtest/gtest.h>
#include <zlib.h>

#include <fmt/ostream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "../spectator/gzip.h"
#include "../spectator/http_client.h"
#include "../spectator/logger.h"
#include "../spectator/registry.h"
#include "../spectator/strings.h"
#include "../spectator/timer.h"
#include "http_server.h"
#include "percentile_bucket_tags.inc"

using spectator::Config;
using spectator::DefaultLogger;
using spectator::GetConfiguration;
using spectator::gzip_uncompress;
using spectator::HttpClient;
using spectator::HttpClientConfig;
using spectator::HttpResponse;
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

static HttpClientConfig get_cfg(int read_to, int connect_to) {
  using millis = std::chrono::milliseconds;
  return HttpClientConfig{millis(connect_to), millis(read_to), true, true,
                          true};
}

TEST(HttpTest, Post) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, get_cfg(100, 100)};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  client.Post(url, "Content-type: application/json", post_data.c_str(),
              post_data.length());

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server.stop();

  auto timer_for_req = find_timer(&registry, "ipc.client.call", "200");
  ASSERT_TRUE(timer_for_req != nullptr);
  auto expected_tags =
      Tags{{"owner", "spectator-cpp"}, {"http.status", "200"},
           {"http.method", "POST"},    {"ipc.status", "success"},
           {"ipc.result", "success"},  {"ipc.attempt", "initial"},
           {"ipc.endpoint", "/foo"},   {"ipc.attempt.final", "true"}};

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

TEST(HttpTest, PostUncompressed) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  auto cfg = get_cfg(100, 100);
  cfg.compress = false;
  HttpClient client{&registry, cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto resp = client.Post(url, "Content-type: application/json",
                          post_data.c_str(), post_data.length());

  EXPECT_EQ(resp.status, 200);
  EXPECT_EQ(resp.raw_body, std::string("OK\n"));

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server.stop();

  auto timer_for_req = find_timer(&registry, "ipc.client.call", "200");
  ASSERT_TRUE(timer_for_req != nullptr);
  auto expected_tags =
      Tags{{"owner", "spectator-cpp"}, {"http.status", "200"},
           {"http.method", "POST"},    {"ipc.status", "success"},
           {"ipc.result", "success"},  {"ipc.attempt", "initial"},
           {"ipc.endpoint", "/foo"},   {"ipc.attempt.final", "true"}};

  const auto& actual_tags = timer_for_req->MeterId()->GetTags();
  EXPECT_EQ(expected_tags, actual_tags);

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), 1);

  const auto& r = requests[0];
  EXPECT_EQ(r.method(), "POST");
  EXPECT_EQ(r.path(), "/foo");
  EXPECT_EQ(r.get_header("Content-Type"), "application/json");

  const auto src = r.body();
  const auto src_len = r.size();

  std::string body_str{src, src_len};
  EXPECT_EQ(post_data, body_str);
}

TEST(HttpTest, Timeout) {
  http_server server;
  server.set_read_sleep(std::chrono::milliseconds(100));
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  TestRegistry registry{GetConfiguration()};
  auto logger = registry.GetLogger();
  logger->info("Server started on port {}", port);

  HttpClient client{&registry, get_cfg(10, 10)};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto response = client.Post(url, "Content-type: application/json",
                              post_data.c_str(), post_data.length());
  server.stop();

  auto expected_response = HttpResponse{400, ""};
  ASSERT_EQ(response.status, expected_response.status);
  ASSERT_EQ(response.raw_body, expected_response.raw_body);
  auto timer_for_req = find_timer(&registry, "ipc.client.call", "-1");
  ASSERT_TRUE(timer_for_req != nullptr);

  auto expected_tags =
      Tags{{"owner", "spectator-cpp"}, {"http.status", "-1"},
           {"ipc.result", "failure"},  {"ipc.status", "timeout"},
           {"ipc.attempt", "initial"}, {"ipc.attempt.final", "true"},
           {"ipc.endpoint", "/foo"},   {"http.method", "POST"}};
  EXPECT_EQ(expected_tags, timer_for_req->MeterId()->GetTags());
}

TEST(HttpTest, ConnectTimeout) {
  using spectator::HttpClient;
  TestRegistry registry{GetConfiguration()};
  auto logger = registry.GetLogger();

  HttpClient client{&registry, get_cfg(100, 100)};
  const std::string url = "http://192.168.255.255:81/foo";
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto response = client.Post(url, "Content-type: application/json", post_data);

  auto expected_response = HttpResponse{400, ""};
  ASSERT_EQ(response.status, expected_response.status);
  ASSERT_EQ(response.raw_body, expected_response.raw_body);

  auto meters = registry.Meters();
  for (const auto& m : meters) {
    logger->info("{}", m->MeterId()->GetTags());
  }
}

TEST(HttpTest, PostJson) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  auto cfg = get_cfg(5000, 5000);
  cfg.compress = false;
  cfg.read_body = false;
  HttpClient client{&registry, cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  rapidjson::Document payload{rapidjson::kObjectType};
  for (auto i = 0; i < 128; i++) {
    auto key = fmt::format("key-{}", i);
    auto value = fmt::format("value-{}", i);
    rapidjson::Value k{key.c_str(), payload.GetAllocator()};
    rapidjson::Value v{value.c_str(), payload.GetAllocator()};
    payload.AddMember(k, v, payload.GetAllocator());
  }
  // not interested in the output from the server
  auto resp = client.Post(url, payload);
  EXPECT_EQ(resp.raw_body, "");
  EXPECT_EQ(resp.status, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server.stop();

  auto timer_for_req = find_timer(&registry, "ipc.client.call", "200");
  ASSERT_TRUE(timer_for_req != nullptr);
  auto expected_tags =
      Tags{{"owner", "spectator-cpp"}, {"http.status", "200"},
           {"http.method", "POST"},    {"ipc.status", "success"},
           {"ipc.result", "success"},  {"ipc.attempt", "initial"},
           {"ipc.endpoint", "/foo"},   {"ipc.attempt.final", "true"}};

  const auto& actual_tags = timer_for_req->MeterId()->GetTags();
  EXPECT_EQ(expected_tags, actual_tags);

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), 1);

  const auto& r = requests[0];
  EXPECT_EQ(r.method(), "POST");
  EXPECT_EQ(r.path(), "/foo");
  EXPECT_EQ(r.get_header("Content-Type"), "application/json");

  const auto src = r.body();
  const auto src_len = r.size();
  std::string body_str{src, src_len};

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
  payload.Accept(writer);
  std::string post_data{buffer.GetString()};
  EXPECT_EQ(post_data, body_str);
}

TEST(HttpTest, PostHeaders) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  auto cfg = get_cfg(5000, 5000);
  cfg.compress = false;
  cfg.read_body = true;
  cfg.read_headers = true;
  HttpClient client{&registry, cfg};
  auto url = fmt::format("http://localhost:{}/hdr", port);
  std::string payload{"stuff"};
  // not interested in the output from the server
  auto resp = client.Post(url, "Content-Type: text/plain", payload);
  auto expected_body = std::string("header body: ok\n");
  EXPECT_EQ(resp.raw_body, expected_body);
  EXPECT_EQ(resp.status, 200);

  auto expected_len = fmt::format("{}", expected_body.length());
  EXPECT_EQ(resp.headers.size(), 2);
  EXPECT_EQ(resp.headers["Content-Length"], expected_len);
  EXPECT_EQ(resp.headers["X-Test"], "some server");
  server.stop();
}

TEST(HttpTest, Get) {
  auto cfg = get_cfg(5000, 5000);
  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, cfg};

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  auto response = client.Get(fmt::format("http://localhost:{}/get", port));
  server.stop();
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.raw_body, "InsightInstanceProfile");

  EXPECT_EQ(registry.Meters().size(), 2);

  spectator::Tags timer_tags{
      {"http.status", "200"},    {"ipc.attempt", "initial"},
      {"ipc.result", "success"}, {"owner", "spectator-cpp"},
      {"ipc.status", "success"}, {"ipc.attempt.final", "true"},
      {"http.method", "GET"},    {"ipc.endpoint", "/get"}};
  auto timer_id = registry.CreateId("ipc.client.call", timer_tags);
  auto timer = registry.GetTimer(timer_id);
  EXPECT_EQ(timer->Count(), 1);
}

TEST(HttpTest, Get503) {
  auto cfg = get_cfg(5000, 5000);
  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, cfg};

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);

  auto response = client.Get(fmt::format("http://localhost:{}/get503", port));
  server.stop();
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.raw_body, "InsightInstanceProfile");

  // 2 percentile timers, one for success, one for error
  EXPECT_EQ(registry.Meters().size(), 4);

  spectator::Tags err_timer_tags{
      {"http.status", "503"},       {"ipc.attempt", "initial"},
      {"ipc.result", "failure"},    {"owner", "spectator-cpp"},
      {"ipc.status", "http_error"}, {"ipc.attempt.final", "false"},
      {"http.method", "GET"},       {"ipc.endpoint", "/get503"}};
  spectator::Tags success_timer_tags{
      {"http.status", "200"},    {"ipc.attempt", "second"},
      {"ipc.result", "success"}, {"owner", "spectator-cpp"},
      {"ipc.status", "success"}, {"ipc.attempt.final", "true"},
      {"http.method", "GET"},    {"ipc.endpoint", "/get503"}};
  auto err_id = registry.CreateId("ipc.client.call", err_timer_tags);
  auto err_timer = registry.GetTimer(err_id);
  EXPECT_EQ(err_timer->Count(), 1);

  auto success_id = registry.CreateId("ipc.client.call", success_timer_tags);
  auto success_timer = registry.GetTimer(success_id);
  EXPECT_EQ(success_timer->Count(), 1);
}

TEST(HttpTest, GetHeader) {
  auto cfg = get_cfg(5000, 5000);
  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, cfg};

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = DefaultLogger();
  logger->info("Server started on port {}", port);
  std::vector<std::string> headers{"X-Spectator: foo", "X-Other-Header: bar"};

  auto response =
      client.Get(fmt::format("http://localhost:{}/getheader", port), headers);
  server.stop();
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.raw_body, "x-other-header: bar\nx-spectator: foo\n");

  EXPECT_EQ(registry.Meters().size(), 2);

  spectator::Tags timer_tags{
      {"http.status", "200"},    {"ipc.attempt", "initial"},
      {"ipc.result", "success"}, {"owner", "spectator-cpp"},
      {"ipc.status", "success"}, {"ipc.attempt.final", "true"},
      {"http.method", "GET"},    {"ipc.endpoint", "/getheader"}};
  auto timer_id = registry.CreateId("ipc.client.call", timer_tags);
  auto timer = registry.GetTimer(timer_id);
  EXPECT_EQ(timer->Count(), 1);
}
