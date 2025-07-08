#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <mutex>

// Functions to interact with the UDP server's message storage
std::vector<std::string> get_messages();
void clear_messages();
void add_message(const std::string& message);

// Function to run the server in a thread - can be used by both the server executable and tests
void listen_for_udp_messages();

// Flag to control server shutdown - used to gracefully stop the server
extern std::atomic<bool> server_running;
