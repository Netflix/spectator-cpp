#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace perf {

// The socket writer that the library sends through.
enum class WriterKind
{
    Udp,
    Unix
};

// How the recording threads call the library.
enum class Load
{
    FlatOut,  // every thread records as fast as it can
    Paced     // every thread records a fixed number of lines per second
};

inline std::string WriterName(WriterKind kind) { return kind == WriterKind::Udp ? "udp" : "unix"; }

inline std::string LoadName(Load load) { return load == Load::FlatOut ? "flat-out" : "paced"; }

// One configuration to measure. To measure a new setting of the writer, add it here, to Key(), and to the places
// that hand a scenario to the recording process (ToArguments and FromArguments below) and to the library
// (recording_run.cpp).
struct Scenario
{
    WriterKind writer = WriterKind::Unix;
    unsigned int bufferSize = 0;  // 0: no buffering, the library sends every line as it is recorded
    int threads = 1;              // recording threads
    Load load = Load::FlatOut;
    int linesPerSecond = 0;       // paced load: the rate of each thread
    bool createMeter = false;     // create the counter for every line, instead of once per thread
    bool tags = true;             // whether the counter has tags

    // Names the scenario in the CSV file. Two runs are compared scenario by scenario through this.
    std::string Key() const
    {
        std::string key = WriterName(writer) + "/buffer=" + std::to_string(bufferSize) +
                          "/threads=" + std::to_string(threads) + "/" + LoadName(load);
        if (load == Load::Paced)
        {
            key += "=" + std::to_string(linesPerSecond);
        }
        key += createMeter ? "/create" : "/reuse";
        key += tags ? "/tags" : "/no-tags";
        return key;
    }
};

// Settings in the form name=value, which is how the test hands a scenario to the process that records, and how that
// process hands back what it measured.
using Arguments = std::map<std::string, std::string>;

inline Arguments ParseArguments(const std::vector<std::string>& words)
{
    Arguments arguments;
    for (const std::string& word : words)
    {
        const std::size_t equals = word.find('=');
        if (equals == std::string::npos)
        {
            throw std::invalid_argument("Expected name=value, got: " + word);
        }
        arguments[word.substr(0, equals)] = word.substr(equals + 1);
    }
    return arguments;
}

inline const std::string& Text(const Arguments& arguments, const std::string& name)
{
    const auto found = arguments.find(name);
    if (found == arguments.end())
    {
        throw std::invalid_argument("Missing setting: " + name);
    }
    return found->second;
}

inline double Number(const Arguments& arguments, const std::string& name) { return std::stod(Text(arguments, name)); }

inline std::uint64_t Count(const Arguments& arguments, const std::string& name)
{
    return std::stoull(Text(arguments, name));
}

inline std::vector<std::string> ToArguments(const Scenario& scenario)
{
    return {
        "writer=" + WriterName(scenario.writer),
        "buffer=" + std::to_string(scenario.bufferSize),
        "threads=" + std::to_string(scenario.threads),
        "load=" + LoadName(scenario.load),
        "rate=" + std::to_string(scenario.linesPerSecond),
        "create=" + std::to_string(scenario.createMeter ? 1 : 0),
        "tags=" + std::to_string(scenario.tags ? 1 : 0),
    };
}

inline Scenario FromArguments(const Arguments& arguments)
{
    Scenario scenario;
    scenario.writer = Text(arguments, "writer") == "udp" ? WriterKind::Udp : WriterKind::Unix;
    scenario.bufferSize = static_cast<unsigned int>(Count(arguments, "buffer"));
    scenario.threads = static_cast<int>(Count(arguments, "threads"));
    scenario.load = Text(arguments, "load") == "paced" ? Load::Paced : Load::FlatOut;
    scenario.linesPerSecond = static_cast<int>(Count(arguments, "rate"));
    scenario.createMeter = Count(arguments, "create") != 0;
    scenario.tags = Count(arguments, "tags") != 0;
    return scenario;
}

}  // namespace perf
