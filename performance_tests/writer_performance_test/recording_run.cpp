#include "recording_run.h"

#include "histogram.h"

#include <registry.h>

#include <sys/resource.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace perf {

namespace {

using Clock = std::chrono::steady_clock;

// What the recording threads are to do. Only the main thread changes it, so reading it costs the threads nothing.
enum Phase : int
{
    kWaiting,     // until every thread is ready
    kWarmingUp,   // recording, not counted
    kMeasuring,   // recording, counted
    kStopping
};

// Flat out, one call in this many is timed. Reading the clock twice takes about as long as the call itself, so
// timing every call would slow the threads down and change what is measured.
constexpr std::uint64_t kTimedCallEvery = 32;

// How often a paced thread catches up with its rate.
constexpr std::chrono::milliseconds kPaceStep{1};

const std::string kCounterName = "perf_test_counter";

std::uint64_t Nanos(Clock::duration duration)
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

// A time as the value of a line: microseconds on the steady clock, which every process on the machine reads the
// same. The receiver takes the delay of the line from it.
double Micros(Clock::time_point time)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count());
}

// What one thread did. Aligned so that no two threads write to the same cache line.
struct alignas(128) ThreadResult
{
    std::uint64_t calls = 0;          // all calls, warm-up included
    std::uint64_t measuredCalls = 0;  // the calls in the measurement
    double measuredSeconds = 0;       // how long the thread was in the measurement
    Histogram callNanos;              // how long the timed calls in the measurement took
};

// Records as fast as the thread can. `recordLine` records one line with the given value.
template <typename RecordLine>
void RecordFlatOut(const std::atomic<int>& phase, const RecordLine& recordLine, ThreadResult& result)
{
    // Everything the loop writes is local to the thread. The result is filled in afterwards.
    Histogram callNanos;
    std::uint64_t calls = 0;
    std::uint64_t measuredCalls = 0;
    bool measuring = false;
    Clock::time_point measuringSince;

    for (;;)
    {
        const int current = phase.load(std::memory_order_relaxed);
        if (current == kStopping)
        {
            break;
        }
        if (current == kMeasuring && !measuring)
        {
            measuring = true;
            measuringSince = Clock::now();
        }

        if (calls % kTimedCallEvery == 0)
        {
            const Clock::time_point before = Clock::now();
            recordLine(1.0);
            const Clock::time_point after = Clock::now();
            if (measuring)
            {
                callNanos.Record(Nanos(after - before));
            }
        }
        else
        {
            recordLine(1.0);
        }
        ++calls;
        measuredCalls += measuring ? 1 : 0;
    }

    result.calls = calls;
    result.measuredCalls = measuredCalls;
    result.measuredSeconds = measuring ? std::chrono::duration<double>(Clock::now() - measuringSince).count() : 0.0;
    result.callNanos = callNanos;
}

// Records `linesPerSecond` lines per second: every kPaceStep, the thread records the lines that are due by then. The
// value of each line is the time at which it is recorded. Every call is timed.
template <typename RecordLine>
void RecordPaced(const std::atomic<int>& phase, int linesPerSecond, const RecordLine& recordLine,
                 ThreadResult& result)
{
    Histogram callNanos;
    std::uint64_t calls = 0;
    std::uint64_t measuredCalls = 0;
    bool measuring = false;
    Clock::time_point measuringSince;
    const Clock::time_point start = Clock::now();

    for (;;)
    {
        const int current = phase.load(std::memory_order_relaxed);
        if (current == kStopping)
        {
            break;
        }
        if (current == kMeasuring && !measuring)
        {
            measuring = true;
            measuringSince = Clock::now();
        }

        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        const auto due = static_cast<std::uint64_t>(elapsed * linesPerSecond);
        while (calls < due)
        {
            const Clock::time_point before = Clock::now();
            recordLine(Micros(before));
            const Clock::time_point after = Clock::now();
            if (measuring)
            {
                callNanos.Record(Nanos(after - before));
                ++measuredCalls;
            }
            ++calls;
        }
        std::this_thread::sleep_for(kPaceStep);
    }

    result.calls = calls;
    result.measuredCalls = measuredCalls;
    result.measuredSeconds = measuring ? std::chrono::duration<double>(Clock::now() - measuringSince).count() : 0.0;
    result.callNanos = callNanos;
}

// One recording thread.
void Record(const Scenario& scenario, const spectator::Registry& registry, const std::atomic<int>& phase,
            std::atomic<int>& ready, ThreadResult& result)
{
    // The tags of the existing performance test.
    const std::unordered_map<std::string, std::string> tags{{"location", WriterName(scenario.writer)},
                                                            {"version", "correct-horse-battery-staple"}};

    const auto run = [&](const auto& recordLine)
    {
        ready.fetch_add(1);
        while (phase.load(std::memory_order_acquire) == kWaiting)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        if (scenario.load == Load::Paced)
        {
            RecordPaced(phase, scenario.linesPerSecond, recordLine, result);
        }
        else
        {
            RecordFlatOut(phase, recordLine, result);
        }
    };

    if (!scenario.createMeter)
    {
        // The counter exists before the measurement begins, so the loop is the call that records and nothing else.
        const spectator::Counter counter =
            scenario.tags ? registry.CreateCounter(kCounterName, tags) : registry.CreateCounter(kCounterName);
        run([&counter](double value) { counter.Increment(value); });
    }
    else if (scenario.tags)
    {
        // What a caller does that writes registry.CreateCounter(name, tags).Increment() wherever it counts.
        run([&registry, &tags](double value) { registry.CreateCounter(kCounterName, tags).Increment(value); });
    }
    else
    {
        run([&registry](double value) { registry.CreateCounter(kCounterName).Increment(value); });
    }
}

// How long reading the clock takes, in nanoseconds: what a timed call is measured too long by. It is the average
// over many readings, because one reading can be shorter than the steps in which the clock advances.
double ClockNanos()
{
    constexpr int kReadings = 100000;
    const Clock::time_point begin = Clock::now();
    Clock::time_point last = begin;
    for (int i = 0; i < kReadings; ++i)
    {
        last = Clock::now();
    }
    return static_cast<double>(Nanos(last - begin)) / kReadings;
}

// The processor time that this process has used, in seconds, on all its threads.
double ProcessorSeconds()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    const auto seconds = [](const timeval& time)
    { return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_usec) / 1e6; };
    return seconds(usage.ru_utime) + seconds(usage.ru_stime);
}

}  // namespace

int RunRecording(const Arguments& arguments)
try
{
    const Scenario scenario = FromArguments(arguments);
    const std::string& location = Text(arguments, "location");
    const std::chrono::duration<double> warmUp(Number(arguments, "warmup"));
    const std::chrono::duration<double> measurement(Number(arguments, "seconds"));

    // The lines go where this test listens, whatever the environment says.
    unsetenv("SPECTATOR_OUTPUT_LOCATION");
    // What the library logs is not part of what is measured, and a send that fails shows in the lines that arrive.
    spectator::SetLogLevel(spdlog::level::off);

    // No tags from the environment, so that the lines are the same on every machine.
    const spectator::WriterConfig writerConfig = scenario.bufferSize == 0
                                                     ? spectator::WriterConfig(location)
                                                     : spectator::WriterConfig(location, scenario.bufferSize);
    const spectator::Config config(writerConfig, {}, false);
    const spectator::Registry registry(config);

    const double clockNanos = ClockNanos();

    std::atomic<int> phase{kWaiting};
    std::atomic<int> ready{0};
    std::vector<ThreadResult> results(static_cast<std::size_t>(scenario.threads));
    std::vector<std::thread> threads;
    for (ThreadResult& result : results)
    {
        threads.emplace_back(Record, std::cref(scenario), std::cref(registry), std::cref(phase), std::ref(ready),
                             std::ref(result));
    }
    while (ready.load() < scenario.threads)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    const Clock::time_point start = Clock::now();
    phase.store(kWarmingUp);
    std::this_thread::sleep_for(warmUp);
    phase.store(kMeasuring);
    std::this_thread::sleep_for(measurement);
    phase.store(kStopping);
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();

    // Each thread's rate over the time that the thread itself measured, so a thread that was held up at either end
    // of the measurement is neither cut short nor counted for too long.
    std::uint64_t recorded = 0;
    std::uint64_t measured = 0;
    double linesPerSecond = 0;
    Histogram callNanos;
    for (const ThreadResult& result : results)
    {
        recorded += result.calls;
        measured += result.measuredCalls;
        if (result.measuredSeconds > 0)
        {
            linesPerSecond += static_cast<double>(result.measuredCalls) / result.measuredSeconds;
        }
        callNanos.Merge(result.callNanos);
    }

    std::printf("%srecorded=%llu measured=%llu rate=%.1f call_p50=%llu call_p99=%llu call_p999=%llu call_max=%llu "
                "timed=%llu seconds=%.4f cpu=%.4f clock=%.1f\n",
                kResultPrefix, static_cast<unsigned long long>(recorded), static_cast<unsigned long long>(measured),
                linesPerSecond, static_cast<unsigned long long>(callNanos.Percentile(0.5)),
                static_cast<unsigned long long>(callNanos.Percentile(0.99)),
                static_cast<unsigned long long>(callNanos.Percentile(0.999)),
                static_cast<unsigned long long>(callNanos.Max()), static_cast<unsigned long long>(callNanos.Count()),
                seconds, ProcessorSeconds(), clockNanos);
    std::fflush(stdout);
    return 0;
}
catch (const std::exception& error)
{
    std::fprintf(stderr, "The recording process failed: %s\n", error.what());
    return 2;
}

}  // namespace perf
