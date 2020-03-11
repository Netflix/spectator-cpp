#pragma once

#include "../spectator/registry.h"
#include <map>
#include <string>
#include <vector>

std::map<std::string, double> measurements_to_map(
    const std::vector<spectator::Measurement>& measurements);

using MeterPtr = std::shared_ptr<spectator::Meter>;
inline std::vector<MeterPtr> my_meters(const spectator::Registry& registry) {
  std::vector<MeterPtr> result;
  auto meters = registry.Meters();
  std::copy_if(meters.begin(), meters.end(), std::back_inserter(result),
               [](const MeterPtr& m) {
                 auto found =
                     m->MeterId()->Name().rfind("spectator.measurements", 0);
                 return found == std::string::npos;
               });
  return result;
}
