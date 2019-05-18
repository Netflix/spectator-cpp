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

std::string PathFromUrl(const std::string& url) noexcept {
  if (url.empty()) {
    return "/";
  }

  auto proto_end = std::find(url.begin(), url.end(), ':');
  if (proto_end == url.end()) {
    return url;  // no protocol, assume just a path
  }

  std::string protocol = &*(proto_end);
  if (protocol.length() < 3) {
    return url;
  }
  proto_end += 3;  // skip over ://

  auto path_begin = std::find(proto_end, url.end(), '/');
  if (path_begin == url.end()) {
    return "/";
  }

  auto query_begin = std::find(path_begin, url.end(), '?');
  return std::string{path_begin, query_begin};
}

}  // namespace spectator
