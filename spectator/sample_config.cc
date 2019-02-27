#include "config.h"
#include "memory.h"

namespace spectator {

std::unique_ptr<Config> GetConfiguration() {
  return std::make_unique<Config>();
}

}  // namespace spectator
