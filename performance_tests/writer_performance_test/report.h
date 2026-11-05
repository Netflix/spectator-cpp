#pragma once

#include "plan.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace perf {

// What one measurement of a scenario gave.
struct Measurement
{
    bool ok = false;
    std::string problem;  // why it is not ok

    double linesPerSecond = 0;  // recorded during the measurement, by all threads together
    double callP50 = 0;         // how long a call that records a line took, in nanoseconds
    double callP99 = 0;
    double callP999 = 0;
    double callMax = 0;
    double clockNanos = 0;       // how long reading the clock takes, which the call times include
    double cpuNanosPerLine = 0;  // processor time of the recording process, on all its threads

    std::uint64_t recorded = 0;  // lines, warm-up included
    std::uint64_t received = 0;  // lines
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;

    double delayP50 = 0;  // paced load: from recording a line to its arrival, in microseconds
    double delayP99 = 0;
    double delayMax = 0;
};

// The measurements of a run: for every scenario of the plan, one per repeat.
using Measurements = std::vector<std::vector<Measurement>>;

// A rate in words, for example 12.35 M or 853.1 k.
std::string Rate(double perSecond);

// What is being run, on what, and for how long. Warns if this program was built without optimization.
void PrintHeader(const std::string& label, const Selection& selection, const Plan& plan);

// The tables and the notes below them. With a baseline (see ReadBaseline), the rates are also shown as a multiple
// of the baseline's.
void PrintReport(const Plan& plan, const Measurements& measurements, const std::map<std::string, double>& baseline);

// Whether every line that was recorded in these measurements arrived.
bool EveryLineArrived(const std::vector<Measurement>& measurements);

// Writes every measurement to a file of comma-separated values, one row each.
void WriteCsv(const std::string& path, const std::string& label, const Plan& plan, const Measurements& measurements);

// From a file that WriteCsv wrote: the median rate of every scenario, by Scenario::Key().
std::map<std::string, double> ReadBaseline(const std::string& path);

}  // namespace perf
