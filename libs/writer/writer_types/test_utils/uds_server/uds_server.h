#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

// The socket path used when none is given. Test binaries that run at the same time must each pass their own path.
inline constexpr const char* kDefaultUdsServerPath = "/tmp/test_uds_socket";

// Functions to interact with the UDS server's message storage. Each received datagram is one message.
std::vector<std::string> get_uds_messages();
void clear_uds_messages();
void add_uds_message(const std::string& message);

// Receives datagrams on socketPath until uds_server_running is false. Run it on its own thread.
void listen_for_uds_messages(const std::string& socketPath = kDefaultUdsServerPath);

// Waits until listen_for_uds_messages has bound its socket. Returns false if that does not happen in time.
bool wait_for_uds_server(std::chrono::milliseconds timeout = std::chrono::seconds(5));

// Flag to control server shutdown - used to gracefully stop the server
extern std::atomic<bool> uds_server_running;
