#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

namespace spectator {

class Config {
 public:
  virtual ~Config() = default;
  Config() = default;
  Config(const Config&) = default;

  Config(std::map<std::string, std::string> c_tags, int read_to_ms,
         int connect_to_ms, int batch_sz, int freq_ms, std::string publish_uri)
      : common_tags{std::move(c_tags)},
        read_timeout{std::chrono::milliseconds{read_to_ms}},
        connect_timeout{std::chrono::milliseconds{connect_to_ms}},
        batch_size{batch_sz},
        frequency{std::chrono::milliseconds{freq_ms}},
        uri{std::move(publish_uri)} {}
  std::map<std::string, std::string> common_tags;
  std::chrono::milliseconds read_timeout;
  std::chrono::milliseconds connect_timeout;
  int batch_size;
  std::chrono::milliseconds frequency;
  std::string uri;

  // sub-classes can override this method implementing custom logic
  // that can disable publishing under certain conditions
  virtual bool is_enabled() const { return true; }
};

// Get a new spectator configuration.
std::unique_ptr<Config> GetConfiguration();

}  // namespace spectator
