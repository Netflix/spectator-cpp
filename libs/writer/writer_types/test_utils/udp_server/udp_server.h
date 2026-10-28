#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

// The port used when none is given. Test binaries that run at the same time must each pass their own port.
inline constexpr int kDefaultUdpServerPort = 12345;

// Functions to interact with the UDP server's message storage. Each received datagram is one message.
std::vector<std::string> get_udp_messages();
void clear_udp_messages();
void add_message(const std::string& message);

// Receives datagrams on 127.0.0.1:port until udp_server_running is false. Run it on its own thread.
void listen_for_udp_messages(int port = kDefaultUdpServerPort);

// Waits until listen_for_udp_messages has bound its socket. Returns false if that does not happen in time.
bool wait_for_udp_server(std::chrono::milliseconds timeout = std::chrono::seconds(5));

// Flag to control server shutdown - used to gracefully stop the server
extern std::atomic<bool> udp_server_running;
