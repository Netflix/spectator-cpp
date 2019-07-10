#pragma once

#include <memory>
#include <rapidjson/document.h>

namespace spectator {

std::shared_ptr<char> JsonGetString(const rapidjson::Document& document);

}  // namespace spectator
