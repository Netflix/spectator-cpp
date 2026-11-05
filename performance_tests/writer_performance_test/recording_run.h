#pragma once

#include "scenario.h"

namespace perf {

// The line that RunRecording() prints starts with this. The rest of it is settings in the form name=value.
inline constexpr const char* kResultPrefix = "PERF_RESULT ";

// The recording side of one measurement. It runs in a process of its own, because the library's Writer is one per
// process: the process creates a Registry for the scenario in `arguments`, which sends to the location given there,
// lets the scenario's threads record during the warm-up and the measurement, and prints what it measured as one
// line. Returns the exit code of the process.
//
// Lines that are still buffered when this returns are sent by the library as the process exits, so a normal exit is
// part of every measurement.
int RunRecording(const Arguments& arguments);

}  // namespace perf
