#pragma once

#include "scenario.h"

#include <string>
#include <vector>

namespace perf {

// The buffer size of the existing performance test. The thread counts and the ways to use a meter are compared at
// this size.
inline constexpr unsigned int kUsualBuffer = 4096;

// Which scenarios a run consists of, and how long each is measured. The default is the short run.
struct Selection
{
    double seconds = 1.0;       // how long a scenario is measured under a flat-out load
    double pacedSeconds = 2.0;  // the same under a paced load, where fewer lines make up the percentiles
    double warmUp = 0.3;        // how long the threads record before the measurement begins
    int repeats = 3;            // how often every scenario is measured

    std::vector<WriterKind> writers{WriterKind::Udp, WriterKind::Unix};

    // Flat out. Buffer size 0 is no buffering, which the library allows from one thread only.
    std::vector<int> threads{1, 4, 8};
    std::vector<unsigned int> buffers{0, 512, kUsualBuffer, 32768};
    // Whether every buffer size is measured with every number of threads. Otherwise the numbers of threads are
    // measured at the usual buffer size, and the buffer sizes with `bufferThreads` threads.
    bool everyCombination = false;
    int bufferThreads = 4;

    // Paced.
    std::vector<int> pacedThreads{1, 4};
    std::vector<unsigned int> pacedBuffers{kUsualBuffer};
    std::vector<int> rates{20000};  // lines per second and thread

    // The full-buffer policies other than the default, measured flat out at the usual buffer size with every
    // number of threads.
    std::vector<Policy> policies{Policy::Drop, Policy::Send};

    // Flush intervals other than the default, in milliseconds, measured under the paced load.
    std::vector<int> flushMillis{100};

    // The ways to use a meter, compared flat out at the usual buffer size.
    std::vector<int> meterThreads{1, 4};
    std::vector<WriterKind> meterWriters{WriterKind::Unix};
};

// More thread counts, buffer sizes and rates, measured for longer.
Selection FullSelection();

// A few seconds, to see that the test itself works: two scenarios at a rate at which no line may be lost.
Selection SmokeSelection();

// The scenarios of a run, and what was left out of it and why.
struct Plan
{
    std::vector<Scenario> scenarios;
    std::vector<std::string> leftOut;
};

// The scenarios of a selection. Buffer sizes that this system's sockets cannot carry are left out.
Plan MakePlan(const Selection& selection);

// How long the scenario is measured.
double MeasuredSeconds(const Scenario& scenario, const Selection& selection);

// How long the whole run takes, roughly.
double EstimatedSeconds(const Plan& plan, const Selection& selection);

// The scenario in words, for example: unix, buffer 4096, 4 threads, flat out.
std::string Describe(const Scenario& scenario);

}  // namespace perf
