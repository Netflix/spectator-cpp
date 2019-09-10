#include "http_server.h"
#include "../spectator/gzip.h"
#include "../spectator/logger.h"
#include <fmt/format.h>
#include <fstream>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

using spectator::DefaultLogger;
using spectator::gzip_compress;

http_server::http_server() noexcept {
  path_response_["/foo"] =
      "HTTP/1.0 200 OK\nContent-Length: 3\nX-Test: foo\n\nOK\n";
  auto hdr_body = std::string("header body: ok\n");
  auto hdr_response = fmt::format(
      "HTTP/1.0 200 OK\nContent-Length: {}\nX-Test: some "
      "server\nX-NoContent:\n\n{}",
      hdr_body.length(), hdr_body);
  path_response_["/hdr"] = hdr_response;

  path_response_["/get"] =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain\n"
      "Accept-Ranges: none\n"
      "Last-Modified: Tue, 10 Sep 2019 18:22:20 GMT\n"
      "Content-Length: 22\n"
      "Date: Tue, 10 Sep 2019 18:23:27 GMT\n"
      "Connection: close\n"
      "\n"
      "InsightInstanceProfile";
}

http_server::~http_server() {
  std::lock_guard<std::mutex> guard(requests_mutex_);
  if (sockfd_ >= 0) {
    close(sockfd_);
  }
}

void http_server::start() noexcept {
  struct sockaddr_in serv_addr;
  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_TRUE(sockfd_ >= 0);

  bzero(&serv_addr, sizeof serv_addr);
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = 0;

  int sockfd = sockfd_;
  ASSERT_TRUE(bind(sockfd, (sockaddr*)&serv_addr, sizeof serv_addr) >= 0);
  ASSERT_TRUE(listen(sockfd, 32) == 0);

  socklen_t serv_len = sizeof serv_addr;
  ASSERT_TRUE(getsockname(sockfd, (sockaddr*)&serv_addr, &serv_len) >= 0);
  port_ = ntohs(serv_addr.sin_port);

  DefaultLogger()->info("Http server started");
  accept_ = std::thread{&http_server::accept_loop, this};
}

static std::string to_lower(const std::string& s) {
  std::string copy{s};
  for (auto i = 0u; i < s.length(); ++i) {
    copy[i] = static_cast<char>(std::tolower(s[i]));
  }
  return copy;
}

std::string http_server::Request::get_header(const std::string& name) const {
  auto lower = to_lower(name);
  auto it = headers_.find(lower);
  if (it == headers_.end()) {
    return "";
  }
  return it->second;
}

const std::vector<http_server::Request>& http_server::get_requests() const {
  std::lock_guard<std::mutex> guard(requests_mutex_);
  return requests_;
};

static void get_line(int client, char* buf, size_t size) {
  assert(size > 0);
  size_t i = 0;
  char c = '\0';

  while ((i < size - 1) && (c != '\n')) {
    auto n = recv(client, &c, 1, 0);
    if (n > 0) {
      if (c == '\r') {
        n = recv(client, &c, 1, MSG_PEEK);
        if (n > 0 && c == '\n') {
          recv(client, &c, 1, 0);
        } else {
          c = '\n';
        }
      }
      buf[i++] = c;
    } else {
      c = '\n';
    }
  }
  buf[i] = '\0';
}

void http_server::accept_request(int client) {
  using namespace std;

  auto logger = DefaultLogger();
  char buf[65536];
  get_line(client, buf, sizeof buf);

  char method[256];
  char path[1024];
  size_t i = 0, j = 0;
  while (!isspace(buf[i]) && i < (sizeof method - 1)) {
    method[i++] = buf[j++];
  }
  method[i] = '\0';
  while (isspace(buf[j]) && j < sizeof buf) {
    ++j;
  }

  i = 0;
  while (!isspace(buf[j]) && i < (sizeof path - 1) && j < sizeof buf) {
    path[i++] = buf[j++];
  }
  path[i] = '\0';

  // do headers
  map<string, string> headers;
  for (;;) {
    get_line(client, buf, sizeof buf);
    if (strlen(buf) <= 1) {
      break;
    }

    i = 0;
    while (buf[i] != ':' && i < (sizeof buf - 1)) {
      buf[i] = (char)tolower(buf[i]);
      ++i;
    }
    buf[i++] = '\0';

    while (isspace(buf[i]) && i < sizeof buf) {
      ++i;
    }

    string header_name = buf;
    j = i;
    while (buf[i] != '\n' && i < (sizeof buf - 1)) {
      ++i;
    }
    buf[i] = '\0';
    string header_value = &buf[j];
    headers[header_name] = header_value;
  }

  logger->debug("Reading body of HTTP {} request", method);
  // do the body
  auto len = headers["content-length"];
  int content_len = len.empty() ? 0 : stoi(len);

  auto body = unique_ptr<char[]>(new char[content_len + 1]);

  char* p = body.get();
  auto n = content_len;
  while (n > 0) {
    auto bytes_read = read(client, p, (size_t)n);
    if (bytes_read == 0) {
      logger->info("EOF while reading request body. {} bytes read, {} missing",
                   content_len - n, n);
      break;
    } else if (bytes_read < 0) {
      logger->error("Error reading client request: {}", strerror(errno));
      break;
    }
    n -= bytes_read;
    p += bytes_read;
  }
  *p = '\0';
  logger->debug("Adding request {} {}", method, path);
  {
    std::lock_guard<std::mutex> guard(requests_mutex_);
    requests_.emplace_back(method, path, headers, content_len, std::move(body));
  }

  if (read_sleep_.count() > 0) {
    std::this_thread::sleep_for(read_sleep_);
  }

  // TODO handle 404s
  const auto& response = path_response_[path];
  auto left_to_write = response.length() + 1;
  auto resp_ptr = response.c_str();
  while (left_to_write > 0) {
    auto written = write(client, resp_ptr, left_to_write);
    resp_ptr += written;
    left_to_write -= written;
  }
}

void http_server::accept_loop() {
  fd_set fds;
  timeval tv{};
  int slept_number = 0;
  while (!is_done) {
    FD_ZERO(&fds);
    FD_SET(sockfd_, &fds);
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    if (accept_sleep_.count() > 0 && slept_number < sleep_number_) {
      DefaultLogger()->debug("Sleeping before accept {} < {}", slept_number,
                             sleep_number_.load());
      std::this_thread::sleep_for(accept_sleep_);
      slept_number++;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 10 * 1000l;
    if (select(sockfd_ + 1, &fds, nullptr, nullptr, &tv) > 0) {
      DefaultLogger()->debug("Http server accepting client connection");
      int client_socket = accept(sockfd_, (sockaddr*)&cli_addr, &cli_len);
      if (client_socket >= 0) {
        accept_request(client_socket);
        close(client_socket);
      }
    }
  }
}
