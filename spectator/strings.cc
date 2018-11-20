#include "strings.h"

namespace spectator {

bool IStartsWith(const std::string& s, const std::string& prefix) noexcept {
  const char* a = s.c_str();
  const char* b = prefix.c_str();
  for (size_t i = 0; i < prefix.size(); ++i) {
    auto source_letter = a[i];
    if (source_letter == '\0') {
      return false;
    }
    auto prefix_letter = tolower(b[i]);
    if (tolower(source_letter) != prefix_letter) {
      return false;
    }
  }
  return true;
}

}  // namespace spectator
