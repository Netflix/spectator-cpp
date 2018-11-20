#pragma once

#include <map>
#include <string>

namespace spectator {

struct Config {
  std::map<std::string, std::string> common_tags;
  int read_timeout;
  int connect_timeout;
  int batch_size;
  int frequency;
  std::string uri;
};

}  // namespace spectator
