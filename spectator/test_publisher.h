#pragma once

#include "publisher.h"
#include <string>
#include <vector>

namespace spectator {
class TestPublisher {
 public:
  void send(std::string_view msg) { messages.emplace_back(msg); }
  std::vector<std::string> SentMessages() { return messages; }
  void Reset() { messages.clear(); }

 private:
  std::vector<std::string> messages;
};
}  // namespace spectator