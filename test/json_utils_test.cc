#include <gtest/gtest.h>
#include "../spectator/json_utils.h"

using spectator::JsonGetString;

TEST(JsonGetString, PointerWorks) {
  rapidjson::Document array{rapidjson::kArrayType};
  array.PushBack(1, array.GetAllocator());
  auto ptr = JsonGetString(array);
  const char* s = ptr.get();
  EXPECT_STREQ(s, "[1]");
}

