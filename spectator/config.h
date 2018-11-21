#pragma once

#include <map>
#include <string>

namespace spectator {

struct Config {
  std::map<std::string, std::string> common_tags;
  int read_timeout;     // in seconds
  int connect_timeout;  // in seconds
  int batch_size;
  int frequency;  // in seconds
  std::string uri;
};

}  // namespace spectator
