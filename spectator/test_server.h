#include <asio.hpp>
#include <string>
#include <vector>
#include "logger.h"

template <typename T>
class TestServer {
 public:
  explicit TestServer(typename T::endpoint endpoint)
      : socket_{context_, endpoint} {}
  void Start() {
    start_receiving();
    runner = std::thread([this]() { context_.run(); });
  }

  void Stop() {
    spectator::DefaultLogger()->info("Stopping test server");
    context_.stop();
    runner.join();
  }

  ~TestServer() {
    if (runner.joinable()) {
      spectator::DefaultLogger()->info(
          "Test server runner was not stopped properly");
      Stop();
    }
  }

  void Reset() { msgs.clear(); }

  [[nodiscard]] std::vector<std::string> GetMessages() const { return msgs; }

 protected:
  std::thread runner;
  asio::io_context context_{};
  typename T::socket socket_;
  char buf[32768];
  std::vector<std::string> msgs;

  void start_receiving() {
    socket_.async_receive(
        asio::buffer(buf, sizeof buf),
        [this](const std::error_code& err, size_t bytes_transferred) {
          assert(!err);
          msgs.emplace_back(std::string(buf, bytes_transferred));
          start_receiving();
        });
  }
};

class TestUdpServer : public TestServer<asio::ip::udp> {
 public:
  TestUdpServer()
      : TestServer{asio::ip::udp::endpoint{asio::ip::udp::v6(), 0}},
        port_{socket_.local_endpoint().port()} {}

  [[nodiscard]] int GetPort() const { return port_; }

 private:
  int port_;
};

class TestUnixServer : public TestServer<asio::local::datagram_protocol> {
 public:
  explicit TestUnixServer(std::string_view path)
      : TestServer{asio::local::datagram_protocol::endpoint{path}} {}
};
