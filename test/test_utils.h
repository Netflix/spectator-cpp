#pragma once

#include "../spectator/measurement.h"
#include <map>
#include <string>
#include <vector>

std::map<std::string, double> measurements_to_map(
    const std::vector<spectator::Measurement>& measurements);
