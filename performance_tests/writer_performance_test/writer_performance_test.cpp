// Measures what it costs to record meters through the writers, for a list of configurations: the socket writer, the
// buffer size, the number of recording threads, the load, and the way the meters are used.
//
// For every configuration (a Scenario, see scenario.h and plan.h), the test
//   - opens a socket of its own on this machine and reads what arrives there (receiver.h),
//   - starts a process that creates a Registry for that socket and lets its threads record (recording_run.h),
//   - and, once that process has exited, compares what was recorded with what arrived.
// The recording runs in a process of its own because the library's Writer is one per process and is set up once.
//
// It reports, per configuration (report.h):
//   - the lines recorded per second, and how that grows with the number of threads,
//   - how long a call that records a line takes: the median, the 99th percentile and the longest,
//   - the processor time per line, on all threads of the recording process,
//   - the share of the recorded lines that arrived, and how many lines a datagram held,
//   - under a paced load, how long a line took from being recorded to arriving.
//
// The numbers only mean something from an optimized build on a machine that is doing nothing else. README.md, next
// to this file, says how to build and run the test and how to read its report.

#include "plan.h"
#include "receiver.h"
#include "recording_run.h"
#include "report.h"
#include "scenario.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

extern char** environ;

namespace {

using perf::Load;
using perf::Measurement;
using perf::Plan;
using perf::Scenario;
using perf::Selection;
using perf::WriterKind;

// ---------- The command line ----------

struct Options
{
    bool full = false;
    bool smoke = false;
    bool list = false;
    bool help = false;
    double seconds = 0;  // 0: as the selection has it
    double warmUp = -1;  // negative: as the selection has it
    int repeats = 0;     // 0: as the selection has it
    int rate = 0;        // 0: as the selection has it
    std::vector<WriterKind> writers;
    std::vector<int> threads;
    std::vector<unsigned int> buffers;
    std::string csvPath;
    std::string baselinePath;
    std::string label;
};

void PrintUsage()
{
    std::printf(
        "Usage: writer_performance_test [options]\n"
        "\n"
        "Measures recording through the writers for a list of configurations and prints a report.\n"
        "Without options it takes about two minutes.\n"
        "\n"
        "  --full             more thread counts, buffer sizes and rates: about twelve minutes\n"
        "  --smoke            a few seconds: checks that the test works, and that every line arrives\n"
        "  --list             print the configurations and how long they take, and stop\n"
        "\n"
        "  --writers LIST     udp, unix or both, for example: --writers unix\n"
        "  --threads LIST     numbers of recording threads, for example: --threads 1,2,4,8,16,32\n"
        "  --buffers LIST     buffer sizes in bytes, 0 for no buffering, for example: --buffers 0,1024,8192\n"
        "                     With --threads or --buffers, every size is measured with every number of threads.\n"
        "                     Use --list to see what a combination of options comes to.\n"
        "  --rate N           lines per second and thread under the paced load\n"
        "  --seconds S        how long each configuration is measured\n"
        "  --warmup S         how long the threads record before the measurement begins\n"
        "  --repeats N        how often each configuration is measured; the report shows the median\n"
        "\n"
        "  --csv FILE         also write every measurement to FILE\n"
        "  --baseline FILE    a file written with --csv by an earlier run: the report shows this run against it\n"
        "  --label TEXT       a name for this run, for the report and the CSV file\n"
        "\n"
        "The numbers say little unless this program was built with optimization, and unless the machine is\n"
        "doing nothing else. How to build it, and how to read the report, is in the README.md next to its source.\n");
}

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

// A number that was given to an option. Anything else is a mistake on the command line.
double NumberFor(const std::string& option, const std::string& text)
{
    try
    {
        std::size_t used = 0;
        const double number = std::stod(text, &used);
        if (used == text.size() && number >= 0)
        {
            return number;
        }
    }
    catch (const std::exception&)
    {
    }
    throw std::invalid_argument(option + " takes numbers that are not negative, not: " + text);
}

Options ParseOptions(const std::vector<std::string>& words)
{
    Options options;
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        const std::string& word = words[i];
        const auto value = [&]() -> const std::string&
        {
            if (i + 1 >= words.size())
            {
                throw std::invalid_argument(word + " needs a value");
            }
            return words[++i];
        };
        const auto number = [&]() { return NumberFor(word, value()); };

        if (word == "--full")
        {
            options.full = true;
        }
        else if (word == "--smoke")
        {
            options.smoke = true;
        }
        else if (word == "--list")
        {
            options.list = true;
        }
        else if (word == "--help" || word == "-h")
        {
            options.help = true;
        }
        else if (word == "--writers")
        {
            for (const std::string& name : Split(value(), ','))
            {
                if (name != "udp" && name != "unix")
                {
                    throw std::invalid_argument("Unknown writer: " + name);
                }
                options.writers.push_back(name == "udp" ? WriterKind::Udp : WriterKind::Unix);
            }
        }
        else if (word == "--threads")
        {
            for (const std::string& count : Split(value(), ','))
            {
                options.threads.push_back(std::max(1, static_cast<int>(NumberFor(word, count))));
            }
        }
        else if (word == "--buffers")
        {
            for (const std::string& size : Split(value(), ','))
            {
                options.buffers.push_back(static_cast<unsigned int>(NumberFor(word, size)));
            }
        }
        else if (word == "--rate")
        {
            options.rate = std::max(1, static_cast<int>(number()));
        }
        else if (word == "--seconds")
        {
            options.seconds = number();
        }
        else if (word == "--warmup")
        {
            options.warmUp = number();
        }
        else if (word == "--repeats")
        {
            options.repeats = std::max(1, static_cast<int>(number()));
        }
        else if (word == "--csv")
        {
            options.csvPath = value();
        }
        else if (word == "--baseline")
        {
            options.baselinePath = value();
        }
        else if (word == "--label")
        {
            options.label = value();
        }
        else
        {
            throw std::invalid_argument("Unknown option: " + word);
        }
    }
    return options;
}

// The selection that the options ask for: one of the three that plan.h has, with what the options change in it.
Selection SelectionFor(const Options& options)
{
    Selection selection =
        options.smoke ? perf::SmokeSelection() : options.full ? perf::FullSelection() : Selection();
    if (options.seconds > 0)
    {
        selection.seconds = options.seconds;
        selection.pacedSeconds = options.seconds;
    }
    if (options.warmUp >= 0)
    {
        selection.warmUp = options.warmUp;
    }
    if (options.repeats > 0)
    {
        selection.repeats = options.repeats;
    }
    if (options.rate > 0)
    {
        selection.rates = {options.rate};
    }
    if (!options.writers.empty())
    {
        selection.writers = options.writers;
        // The meters are compared with the writers that were asked for.
        const bool wasCompared = !selection.meterWriters.empty();
        std::erase_if(selection.meterWriters, [&](WriterKind writer)
                      { return std::find(options.writers.begin(), options.writers.end(), writer) ==
                               options.writers.end(); });
        if (wasCompared && selection.meterWriters.empty())
        {
            selection.meterWriters = {options.writers.front()};
        }
    }
    // The thread counts and buffer sizes that were asked for apply to every part of the run that the selection has.
    if (!options.threads.empty())
    {
        selection.everyCombination = true;
        for (std::vector<int>* threads : {&selection.threads, &selection.pacedThreads, &selection.meterThreads})
        {
            if (!threads->empty())
            {
                *threads = options.threads;
            }
        }
    }
    if (!options.buffers.empty())
    {
        selection.everyCombination = true;
        selection.buffers = options.buffers;
        if (!selection.pacedBuffers.empty())
        {
            // The paced load is for buffered writing, so it takes the sizes other than 0.
            selection.pacedBuffers.clear();
            for (const unsigned int buffer : options.buffers)
            {
                if (buffer != 0)
                {
                    selection.pacedBuffers.push_back(buffer);
                }
            }
        }
    }
    return selection;
}

// ---------- One measurement ----------

struct ProcessResult
{
    bool exited = false;  // by itself, and in time
    int exitCode = -1;
    std::string output;  // what it wrote to its standard output
};

// Runs a program and collects what it prints. The process is killed if it has not exited after `limit`.
ProcessResult RunProcess(const std::string& executable, const std::vector<std::string>& arguments,
                         std::chrono::milliseconds limit)
{
    int ends[2];
    if (::pipe(ends) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "Could not create a pipe");
    }
    ::fcntl(ends[0], F_SETFD, FD_CLOEXEC);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, ends[1]);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const std::string& argument : arguments)
    {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    pid_t process = 0;
    const int error = posix_spawn(&process, executable.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(ends[1]);
    if (error != 0)
    {
        ::close(ends[0]);
        throw std::system_error(error, std::generic_category(), "Could not start " + executable);
    }

    ProcessResult result;
    bool killed = false;
    const auto deadline = std::chrono::steady_clock::now() + limit;
    pollfd waiting{ends[0], POLLIN, 0};
    char text[4096];
    for (;;)
    {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0)
        {
            ::kill(process, SIGKILL);
            killed = true;
            break;
        }
        if (::poll(&waiting, 1, static_cast<int>(std::min<long long>(left.count(), 100))) <= 0)
        {
            continue;
        }
        const ssize_t size = ::read(ends[0], text, sizeof(text));
        if (size <= 0)
        {
            break;  // the process has closed its end: it has exited
        }
        result.output.append(text, static_cast<std::size_t>(size));
    }
    ::close(ends[0]);

    int status = 0;
    ::waitpid(process, &status, 0);
    result.exited = !killed && WIFEXITED(status);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

// Where this program is, to run it again as the recording process.
std::string ExecutablePath(const char* invokedAs)
{
    char path[PATH_MAX];
#if defined(__APPLE__)
    std::uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
    {
        return path;
    }
#elif defined(__linux__)
    const ssize_t length = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length > 0)
    {
        path[length] = '\0';
        return path;
    }
#endif
    return invokedAs;
}

// Measures a scenario once: the recording in a process of its own, the receiving here.
Measurement Measure(const Scenario& scenario, const Selection& selection, const std::string& executable)
{
    Measurement measurement;
    try
    {
        perf::Receiver receiver(scenario.writer);
        receiver.Start(scenario.load == Load::Paced);

        const double seconds = perf::MeasuredSeconds(scenario, selection);
        std::vector<std::string> arguments{"--child"};
        for (const std::string& argument : perf::ToArguments(scenario))
        {
            arguments.push_back(argument);
        }
        arguments.push_back("location=" + receiver.Location());
        arguments.push_back("warmup=" + std::to_string(selection.warmUp));
        arguments.push_back("seconds=" + std::to_string(seconds));

        // Far longer than it takes, so that only a process that hangs is stopped.
        const std::chrono::milliseconds limit(static_cast<long long>((selection.warmUp + seconds + 30) * 1000));
        const ProcessResult process = RunProcess(executable, arguments, limit);
        const perf::Received received = receiver.Stop();

        if (!process.exited)
        {
            measurement.problem = "the recording process crashed or did not exit in time";
            return measurement;
        }
        const std::size_t prefix = process.output.find(perf::kResultPrefix);
        if (process.exitCode != 0 || prefix == std::string::npos)
        {
            measurement.problem = "the recording process failed";
            return measurement;
        }
        const std::size_t begin = prefix + std::string(perf::kResultPrefix).size();
        const perf::Arguments result = perf::ParseArguments(
            Split(process.output.substr(begin, process.output.find('\n', begin) - begin), ' '));

        measurement.linesPerSecond = perf::Number(result, "rate");
        measurement.callP50 = perf::Number(result, "call_p50");
        measurement.callP99 = perf::Number(result, "call_p99");
        measurement.callP999 = perf::Number(result, "call_p999");
        measurement.callMax = perf::Number(result, "call_max");
        measurement.clockNanos = perf::Number(result, "clock");
        measurement.recorded = perf::Count(result, "recorded");
        measurement.cpuNanosPerLine = measurement.recorded == 0 ? 0
                                                                 : perf::Number(result, "cpu") * 1e9 /
                                                                       static_cast<double>(measurement.recorded);

        measurement.received = received.lines;
        measurement.datagrams = received.datagrams;
        measurement.bytes = received.bytes;
        measurement.delayP50 = static_cast<double>(received.delayMicros.Percentile(0.5));
        measurement.delayP99 = static_cast<double>(received.delayMicros.Percentile(0.99));
        measurement.delayMax = static_cast<double>(received.delayMicros.Max());
        measurement.ok = true;
    }
    catch (const std::exception& error)
    {
        measurement.problem = error.what();
    }
    return measurement;
}

// ---------- The run ----------

int Run(const Options& options, const std::string& executable)
{
    const Selection selection = SelectionFor(options);
    const Plan plan = perf::MakePlan(selection);
    const std::map<std::string, double> baseline =
        options.baselinePath.empty() ? std::map<std::string, double>() : perf::ReadBaseline(options.baselinePath);

    perf::PrintHeader(options.label, selection, plan);
    if (options.list)
    {
        for (const Scenario& scenario : plan.scenarios)
        {
            std::printf("  %s\n", perf::Describe(scenario).c_str());
        }
        for (const std::string& reason : plan.leftOut)
        {
            std::printf("  Left out: %s\n", reason.c_str());
        }
        return 0;
    }

    // Every scenario once, then every scenario again, each time beginning with another one. What changes slowly on
    // the machine while the test runs, such as its temperature, then affects all scenarios alike.
    const std::size_t count = plan.scenarios.size();
    const auto repeats = static_cast<std::size_t>(selection.repeats);
    perf::Measurements measurements(count);
    bool failed = false;
    for (std::size_t repeat = 0; repeat < repeats; ++repeat)
    {
        for (std::size_t step = 0; step < count; ++step)
        {
            const std::size_t index = (step + repeat * count / repeats) % count;
            const Scenario& scenario = plan.scenarios[index];
            const Measurement measurement = Measure(scenario, selection, executable);
            measurements[index].push_back(measurement);
            failed = failed || !measurement.ok;

            std::printf("[%3zu/%zu] %-62s %s\n", repeat * count + step + 1, count * repeats,
                        perf::Describe(scenario).c_str(),
                        measurement.ok ? (perf::Rate(measurement.linesPerSecond) + " lines/s").c_str()
                                       : ("FAILED: " + measurement.problem).c_str());
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    perf::PrintReport(plan, measurements, baseline);
    if (!options.csvPath.empty())
    {
        perf::WriteCsv(options.csvPath, options.label, plan, measurements);
        std::printf("\nEvery measurement is in %s\n", options.csvPath.c_str());
    }

    if (options.smoke)
    {
        // At the low rate of the smoke run, no line may be lost with either writer.
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!perf::EveryLineArrived(measurements[i]))
            {
                std::printf("\nSMOKE TEST FAILED: %s: not every recorded line arrived\n",
                            perf::Describe(plan.scenarios[i]).c_str());
                return 1;
            }
        }
        std::printf("\nSmoke test passed: every recorded line arrived.\n");
    }
    return failed ? 1 : 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    const std::vector<std::string> words(argv + 1, argv + argc);
    try
    {
        // The recording side of one measurement, which Measure() starts.
        if (!words.empty() && words.front() == "--child")
        {
            return perf::RunRecording(perf::ParseArguments({words.begin() + 1, words.end()}));
        }

        const Options options = ParseOptions(words);
        if (options.help)
        {
            PrintUsage();
            return 0;
        }
        return Run(options, ExecutablePath(argv[0]));
    }
    catch (const std::invalid_argument& error)
    {
        std::fprintf(stderr, "%s\n\n", error.what());
        PrintUsage();
        return 2;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
