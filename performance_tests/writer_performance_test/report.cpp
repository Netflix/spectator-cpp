#include "report.h"

#include <sys/utsname.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>

namespace perf {

namespace {

double Median(std::vector<double> values)
{
    if (values.empty())
    {
        return 0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 1 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

// The measurements of one scenario, taken together.
struct Summary
{
    int measurements = 0;       // that are ok
    double linesPerSecond = 0;  // the median
    double spread = 0;          // from the slowest to the fastest measurement, as a share of the median
    double callP50 = 0;         // the medians of the measurements' percentiles
    double callP99 = 0;
    double callP999 = 0;
    double callMax = 0;  // the longest call in any measurement
    double cpuNanosPerLine = 0;
    double receivedShare = 0;
    bool allReceived = false;
    double linesPerDatagram = 0;
    double bytesPerLine = 0;
    double delayP50 = 0;
    double delayP99 = 0;
    double delayMax = 0;
};

Summary Summarize(const std::vector<Measurement>& measurements)
{
    Summary summary;
    std::vector<double> rates, p50, p99, p999, cpu, delayP50, delayP99;
    std::uint64_t recorded = 0, received = 0, datagrams = 0, bytes = 0;
    for (const Measurement& measurement : measurements)
    {
        if (!measurement.ok)
        {
            continue;
        }
        ++summary.measurements;
        rates.push_back(measurement.linesPerSecond);
        p50.push_back(measurement.callP50);
        p99.push_back(measurement.callP99);
        p999.push_back(measurement.callP999);
        cpu.push_back(measurement.cpuNanosPerLine);
        delayP50.push_back(measurement.delayP50);
        delayP99.push_back(measurement.delayP99);
        summary.callMax = std::max(summary.callMax, measurement.callMax);
        summary.delayMax = std::max(summary.delayMax, measurement.delayMax);
        recorded += measurement.recorded;
        received += measurement.received;
        datagrams += measurement.datagrams;
        bytes += measurement.bytes;
    }
    if (summary.measurements == 0)
    {
        return summary;
    }

    summary.linesPerSecond = Median(rates);
    const auto [slowest, fastest] = std::minmax_element(rates.begin(), rates.end());
    summary.spread = summary.linesPerSecond > 0 ? (*fastest - *slowest) / summary.linesPerSecond : 0;
    summary.callP50 = Median(p50);
    summary.callP99 = Median(p99);
    summary.callP999 = Median(p999);
    summary.cpuNanosPerLine = Median(cpu);
    summary.delayP50 = Median(delayP50);
    summary.delayP99 = Median(delayP99);
    summary.receivedShare = recorded == 0 ? 0 : static_cast<double>(received) / static_cast<double>(recorded);
    summary.allReceived = received == recorded;
    summary.linesPerDatagram = datagrams == 0 ? 0 : static_cast<double>(received) / static_cast<double>(datagrams);
    summary.bytesPerLine = received == 0 ? 0 : static_cast<double>(bytes) / static_cast<double>(received);
    return summary;
}

std::string Format(const char* format, double value)
{
    char text[64];
    std::snprintf(text, sizeof(text), format, value);
    return text;
}

std::string Time(double nanos)
{
    if (nanos < 1e3)
    {
        return Format("%.0f ns", nanos);
    }
    if (nanos < 1e6)
    {
        return Format("%.1f us", nanos / 1e3);
    }
    if (nanos < 1e9)
    {
        return Format("%.1f ms", nanos / 1e6);
    }
    return Format("%.2f s", nanos / 1e9);
}

// The share of the lines that arrived. It is never rounded up to 100%: that is shown only if every line arrived.
std::string Arrived(const Summary& summary)
{
    if (summary.allReceived)
    {
        return "100%";
    }
    return Format("%.2f%%", std::min(99.99, std::floor(summary.receivedShare * 10000) / 100));
}

std::string Buffer(unsigned int bufferSize) { return bufferSize == 0 ? "none" : std::to_string(bufferSize); }

std::vector<std::string> Split(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    for (std::string part; std::getline(stream, part, separator);)
    {
        parts.push_back(part);
    }
    return parts;
}

// A table whose columns are as wide as their widest entry. The first `textColumns` columns are aligned to the left,
// the numbers after them to the right.
class Table
{
   public:
    Table(std::vector<std::string> header, std::size_t textColumns)
        : m_rows{std::move(header)}, m_textColumns(textColumns)
    {
    }

    void Add(std::vector<std::string> row) { m_rows.push_back(std::move(row)); }

    bool Empty() const { return m_rows.size() == 1; }

    void Print() const
    {
        std::vector<std::size_t> widths(m_rows.front().size(), 0);
        for (const std::vector<std::string>& row : m_rows)
        {
            for (std::size_t column = 0; column < row.size(); ++column)
            {
                widths[column] = std::max(widths[column], row[column].size());
            }
        }
        for (std::size_t rowIndex = 0; rowIndex < m_rows.size(); ++rowIndex)
        {
            std::string line = " ";
            for (std::size_t column = 0; column < m_rows[rowIndex].size(); ++column)
            {
                const std::string& entry = m_rows[rowIndex][column];
                const std::string padding(widths[column] - entry.size(), ' ');
                line += " " + (column < m_textColumns ? entry + padding : padding + entry) + " ";
            }
            std::printf("%s\n", line.c_str());
            if (rowIndex == 0)
            {
                std::string rule = " ";
                for (const std::size_t width : widths)
                {
                    rule += " " + std::string(width, '-') + " ";
                }
                std::printf("%s\n", rule.c_str());
            }
        }
    }

   private:
    std::vector<std::vector<std::string>> m_rows;
    std::size_t m_textColumns;
};

// A counter that the thread created once and that has tags: what the scenarios use that are not about meters.
bool UsualMeter(const Scenario& scenario) { return !scenario.createMeter && scenario.tags; }

}  // namespace

std::string Rate(double perSecond)
{
    if (perSecond >= 1e6)
    {
        return Format("%.2f M", perSecond / 1e6);
    }
    if (perSecond >= 1e3)
    {
        return Format("%.1f k", perSecond / 1e3);
    }
    return Format("%.0f", perSecond);
}

void PrintHeader(const std::string& label, const Selection& selection, const Plan& plan)
{
    utsname system{};
    ::uname(&system);
#if defined(__OPTIMIZE__)
    const bool optimized = true;
#else
    const bool optimized = false;
#endif

    std::printf("Writer performance test%s\n", label.empty() ? "" : (" (" + label + ")").c_str());
    std::printf("  Machine:   %u hardware threads, %s %s, %s\n", std::thread::hardware_concurrency(), system.sysname,
                system.release, system.machine);
    std::printf("  Compiler:  %s, %s\n", __VERSION__, optimized ? "optimized" : "NOT OPTIMIZED");
    std::printf("  Run:       %zu configurations, each measured %d x %.1f s (paced: %.1f s) after %.1f s of warm-up:"
                " about %.0f s\n",
                plan.scenarios.size(), selection.repeats, selection.seconds, selection.pacedSeconds,
                selection.warmUp, EstimatedSeconds(plan, selection));
    if (!optimized)
    {
        std::printf("\n  WARNING: this program was built without optimization, so its numbers say little about a"
                    "\n  release build. performance_tests/writer_performance_test/README.md says how to build one.\n");
    }
    std::printf("\n");
}

void PrintReport(const Plan& plan, const Measurements& measurements, const std::map<std::string, double>& baseline)
{
    const std::vector<Scenario>& scenarios = plan.scenarios;
    std::vector<Summary> summaries;
    double clockNanos = 0;
    int failed = 0;
    for (const std::vector<Measurement>& ofScenario : measurements)
    {
        summaries.push_back(Summarize(ofScenario));
        for (const Measurement& measurement : ofScenario)
        {
            failed += measurement.ok ? 0 : 1;
            clockNanos = std::max(clockNanos, measurement.clockNanos);
        }
    }

    const bool compare = !baseline.empty();
    const auto againstBaseline = [&](const Scenario& scenario, const Summary& summary) -> std::string
    {
        const auto found = baseline.find(scenario.Key());
        if (found == baseline.end() || found->second <= 0)
        {
            return "-";
        }
        return Format("%.2fx", summary.linesPerSecond / found->second);
    };
    // The rate of the same scenario with one thread, or zero if that was not measured.
    const auto withOneThread = [&](Scenario scenario) -> double
    {
        scenario.threads = 1;
        for (std::size_t i = 0; i < scenarios.size(); ++i)
        {
            if (scenarios[i].Key() == scenario.Key() && summaries[i].measurements > 0)
            {
                return summaries[i].linesPerSecond;
            }
        }
        return 0;
    };

    // Flat out.
    std::vector<std::string> header{"writer", "buffer", "threads", "lines/s", "per thread", "speed-up", "spread",
                                    "call p50", "p99", "longest", "cpu/line", "arrived", "lines/datagram"};
    if (compare)
    {
        header.push_back("vs baseline");
    }
    Table flatOut(header, 2);
    for (std::size_t i = 0; i < scenarios.size(); ++i)
    {
        const Scenario& scenario = scenarios[i];
        const Summary& summary = summaries[i];
        if (scenario.load != Load::FlatOut || !UsualMeter(scenario) || summary.measurements == 0)
        {
            continue;
        }
        const double single = withOneThread(scenario);
        std::vector<std::string> row{WriterName(scenario.writer),
                                     Buffer(scenario.bufferSize),
                                     std::to_string(scenario.threads),
                                     Rate(summary.linesPerSecond),
                                     Rate(summary.linesPerSecond / scenario.threads),
                                     single > 0 ? Format("%.2fx", summary.linesPerSecond / single) : "-",
                                     Format("%.0f%%", summary.spread * 100),
                                     Time(summary.callP50),
                                     Time(summary.callP99),
                                     Time(summary.callMax),
                                     Time(summary.cpuNanosPerLine),
                                     Arrived(summary),
                                     Format("%.1f", summary.linesPerDatagram)};
        if (compare)
        {
            row.push_back(againstBaseline(scenario, summary));
        }
        flatOut.Add(row);
    }
    if (!flatOut.Empty())
    {
        std::printf("Flat out: every thread records as fast as it can, with a counter that it created once.\n\n");
        flatOut.Print();
        std::printf("\n");
    }

    // Paced.
    Table paced({"writer", "buffer", "threads", "lines/s per thread", "call p50", "p99", "p99.9", "longest",
                 "delay p50", "delay p99", "longest delay", "arrived"},
                2);
    for (std::size_t i = 0; i < scenarios.size(); ++i)
    {
        const Scenario& scenario = scenarios[i];
        const Summary& summary = summaries[i];
        if (scenario.load != Load::Paced || summary.measurements == 0)
        {
            continue;
        }
        paced.Add({WriterName(scenario.writer), Buffer(scenario.bufferSize), std::to_string(scenario.threads),
                   std::to_string(scenario.linesPerSecond), Time(summary.callP50), Time(summary.callP99),
                   Time(summary.callP999), Time(summary.callMax), Time(summary.delayP50 * 1e3),
                   Time(summary.delayP99 * 1e3), Time(summary.delayMax * 1e3), Arrived(summary)});
    }
    if (!paced.Empty())
    {
        std::printf("Paced: every thread records a fixed number of lines per second. The delay is the time from\n"
                    "recording a line to its arrival.\n\n");
        paced.Print();
        std::printf("\n");
    }

    // The ways to use a meter: the scenarios that were also measured with another use of the meter.
    std::set<std::tuple<WriterKind, unsigned int, int>> compared;
    for (const Scenario& scenario : scenarios)
    {
        if (scenario.load == Load::FlatOut && !UsualMeter(scenario))
        {
            compared.insert({scenario.writer, scenario.bufferSize, scenario.threads});
        }
    }
    std::vector<std::string> meterHeader{"writer", "buffer", "threads", "counter", "tags", "lines/s", "per thread",
                                         "call p50", "p99", "cpu/line", "bytes/line"};
    if (compare)
    {
        meterHeader.push_back("vs baseline");
    }
    Table meters(meterHeader, 5);
    // The uses of a meter next to each other, for every writer, buffer size and number of threads.
    std::vector<std::size_t> order(scenarios.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right)
                     {
                         const auto rank = [&](const Scenario& scenario)
                         { return std::make_tuple(scenario.writer, scenario.bufferSize, scenario.threads,
                                                  scenario.createMeter, !scenario.tags); };
                         return rank(scenarios[left]) < rank(scenarios[right]);
                     });
    for (const std::size_t i : order)
    {
        const Scenario& scenario = scenarios[i];
        const Summary& summary = summaries[i];
        if (scenario.load != Load::FlatOut || summary.measurements == 0 ||
            compared.count({scenario.writer, scenario.bufferSize, scenario.threads}) == 0)
        {
            continue;
        }
        std::vector<std::string> row{WriterName(scenario.writer),
                                     Buffer(scenario.bufferSize),
                                     std::to_string(scenario.threads),
                                     scenario.createMeter ? "created per line" : "created once",
                                     scenario.tags ? "two" : "none",
                                     Rate(summary.linesPerSecond),
                                     Rate(summary.linesPerSecond / scenario.threads),
                                     Time(summary.callP50),
                                     Time(summary.callP99),
                                     Time(summary.cpuNanosPerLine),
                                     Format("%.0f", summary.bytesPerLine)};
        if (compare)
        {
            row.push_back(againstBaseline(scenario, summary));
        }
        meters.Add(row);
    }
    if (!meters.Empty())
    {
        std::printf("Meters: flat out, with the counter created once per thread or for every line, with and without"
                    "\ntags.\n\n");
        meters.Print();
        std::printf("\n");
    }

    std::printf("Notes\n");
    std::printf("  - lines/s is the median of the measurements. spread is how far the slowest and the fastest of them"
                "\n    are apart: a difference between two configurations that is smaller than that is not one.\n");
    std::printf("  - The call times include reading the clock, about %.0f ns here. Flat out, one call in 32 is timed."
                "\n",
                clockNanos);
    std::printf("  - cpu/line is the processor time of the whole recording process, on all its threads, per line.\n");
    std::printf("  - arrived is the share of the recorded lines that this test received.\n");
    if (!paced.Empty())
    {
        std::printf("  - A paced thread records the lines that are due once every millisecond, a few at a time.\n");
    }
    for (const std::string& reason : plan.leftOut)
    {
        std::printf("  - Left out: %s.\n", reason.c_str());
    }
    if (failed > 0)
    {
        std::printf("  - %d measurements FAILED: see the lines above the tables.\n", failed);
    }
}

bool EveryLineArrived(const std::vector<Measurement>& measurements)
{
    return !measurements.empty() &&
           std::all_of(measurements.begin(), measurements.end(), [](const Measurement& measurement)
                       { return measurement.ok && measurement.received == measurement.recorded; });
}

void WriteCsv(const std::string& path, const std::string& label, const Plan& plan, const Measurements& measurements)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("Could not write " + path);
    }
    file << "label,key,writer,buffer,threads,load,rate_per_thread,meter,tags,repeat,ok,lines_per_second,"
            "call_p50_ns,call_p99_ns,call_p999_ns,call_max_ns,clock_ns,cpu_ns_per_line,recorded_lines,"
            "received_lines,datagrams,bytes,delay_p50_us,delay_p99_us,delay_max_us\n";
    for (std::size_t i = 0; i < plan.scenarios.size(); ++i)
    {
        const Scenario& scenario = plan.scenarios[i];
        for (std::size_t repeat = 0; repeat < measurements[i].size(); ++repeat)
        {
            const Measurement& measurement = measurements[i][repeat];
            file << label << ',' << scenario.Key() << ',' << WriterName(scenario.writer) << ','
                 << scenario.bufferSize << ',' << scenario.threads << ',' << LoadName(scenario.load) << ','
                 << scenario.linesPerSecond << ',' << (scenario.createMeter ? "create" : "reuse") << ','
                 << (scenario.tags ? 1 : 0) << ',' << repeat + 1 << ',' << (measurement.ok ? 1 : 0) << ','
                 << Format("%.1f", measurement.linesPerSecond) << ',' << Format("%.0f", measurement.callP50) << ','
                 << Format("%.0f", measurement.callP99) << ',' << Format("%.0f", measurement.callP999) << ','
                 << Format("%.0f", measurement.callMax) << ',' << Format("%.1f", measurement.clockNanos) << ','
                 << Format("%.1f", measurement.cpuNanosPerLine) << ',' << measurement.recorded << ','
                 << measurement.received << ',' << measurement.datagrams << ',' << measurement.bytes << ','
                 << Format("%.0f", measurement.delayP50) << ',' << Format("%.0f", measurement.delayP99) << ','
                 << Format("%.0f", measurement.delayMax) << '\n';
        }
    }
}

std::map<std::string, double> ReadBaseline(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Could not read " + path);
    }
    std::string line;
    std::getline(file, line);
    const std::vector<std::string> header = Split(line, ',');
    const auto column = [&](const std::string& name)
    {
        const auto found = std::find(header.begin(), header.end(), name);
        if (found == header.end())
        {
            throw std::runtime_error(path + " has no column " + name);
        }
        return static_cast<std::size_t>(found - header.begin());
    };
    const std::size_t key = column("key");
    const std::size_t ok = column("ok");
    const std::size_t rate = column("lines_per_second");

    std::map<std::string, std::vector<double>> rates;
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = Split(line, ',');
        if (fields.size() > std::max({key, ok, rate}) && fields[ok] == "1")
        {
            rates[fields[key]].push_back(std::stod(fields[rate]));
        }
    }
    std::map<std::string, double> medians;
    for (const auto& [scenario, values] : rates)
    {
        medians[scenario] = Median(values);
    }
    return medians;
}

}  // namespace perf
