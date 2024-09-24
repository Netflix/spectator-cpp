#include "id.h"

namespace spectator {

class Id {
 public:
  friend std::ostream& operator<<(std::ostream& os, const Id& id) {
    os << fmt::format("{}", id);
    return os;
  }

};


}  // namespace spectator