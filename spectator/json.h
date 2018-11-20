#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace spectator {

inline const char* JsonGetString(rapidjson::StringBuffer& buffer,
                                 const rapidjson::Document& document) {
  using namespace rapidjson;
  Writer<StringBuffer, UTF8<>, UTF8<>, CrtAllocator, kWriteNanAndInfFlag>
      writer(buffer);
  document.Accept(writer);
  return buffer.GetString();
}
}  // namespace spectator
