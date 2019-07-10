#include "json_utils.h"
#include <memory>
#include <algorithm>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace spectator {
struct SharedAllocator {
  std::shared_ptr<char> mem{(char*)nullptr, std::default_delete<char[]>()};

  void* Malloc(size_t size) {
    if (size != 0u) {
      mem.reset(new char[size]);
    } else {
      mem.reset();
    }
    return mem.get();
  }

  void* Realloc(void* /*unused*/, size_t prev_size, size_t new_size) {
    if (new_size == 0u) {
      mem.reset();
    } else {
      auto p = new char[new_size];
      auto size = std::min(prev_size, new_size);
      memcpy(p, mem.get(), size);
      mem.reset(p);
    }
    return mem.get();
  }

  static void Free(void* /*unused*/) {
    // the shared_ptr manages our memory
  }
};

std::shared_ptr<char> JsonGetString(const rapidjson::Document& document) {
  using MyBuffer =
      rapidjson::GenericStringBuffer<rapidjson::UTF8<>, SharedAllocator>;
  MyBuffer buffer;
  rapidjson::Writer<MyBuffer> writer{buffer};
  document.Accept(writer);
  auto& alloc =
      reinterpret_cast<SharedAllocator&>(buffer.stack_.GetAllocator());
  buffer.GetString(); // force a C string
  return alloc.mem;
}

}  // namespace spectator
