#pragma once

#include "publisher.h"
#include <string>
#include <vector>

namespace spectator {
class TestPublisher {
 public:
  void send(std::string_view prefix, std::string_view value) {
    messages.emplace_back(std::string(prefix) + std::string(value));
  }
  std::vector<std::string> SentMessages() { return messages; }
  void Reset() { messages.clear(); }

 private:
  std::vector<std::string> messages;
};
}  // namespace spectator