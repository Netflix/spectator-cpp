#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <mutex>

// Functions to interact with the UDS server's message storage
std::vector<std::string> get_uds_messages();
void clear_uds_messages();
void add_uds_message(const std::string& message);

// Function to run the server in a thread - can be used by both the server executable and tests
void listen_for_uds_messages();

// Flag to control server shutdown - used to gracefully stop the server
extern std::atomic<bool> uds_server_running;