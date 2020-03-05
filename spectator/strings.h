#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <initializer_list>
#include <string>
#include <string.h>

namespace spectator {

/// Return whether the given string starts with a certain prefix using a
/// case insensitive comparison
bool IStartsWith(const std::string& s, const std::string& prefix) noexcept;

/// Remove whitespace from the end of a string
inline void TrimRight(std::string* s) {
  s->erase(std::find_if(s->rbegin(), s->rend(),
                        [](int c) { return !std::isspace(c); })
               .base(),
           s->end());
}

std::string PathFromUrl(const std::string& url) noexcept;

}  // namespace spectator
