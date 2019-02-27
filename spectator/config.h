#pragma once

#include <map>
#include <string>

namespace spectator {

class Config {
 public:
  virtual ~Config() = default;
  Config() = default;
  Config(const Config&) = default;

  Config(std::map<std::string, std::string> c_tags, int read_to, int connect_to,
         int batch_sz, int freq, std::string publish_uri)
      : common_tags{std::move(c_tags)},
        read_timeout{read_to},
        connect_timeout{connect_to},
        batch_size{batch_sz},
        frequency{freq},
        uri{std::move(publish_uri)} {}
  std::map<std::string, std::string> common_tags;
  int read_timeout;     // in seconds
  int connect_timeout;  // in seconds
  int batch_size;
  int frequency;  // in seconds
  std::string uri;
  virtual bool is_enabled() const { return true; }
};

}  // namespace spectator
