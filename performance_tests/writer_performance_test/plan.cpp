#include "plan.h"

#include "receiver.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>

namespace perf {

namespace {

// No line that this test records is longer. The library sends a buffer once it has reached the buffer size, so a
// datagram can be this much larger than the buffer size.
constexpr std::size_t kLongestLine = 128;

// What every measurement takes on top of the warm-up and the measurement: starting the process, and its exit.
constexpr double kOverheadSeconds = 0.3;

}  // namespace

Selection FullSelection()
{
    Selection selection;
    selection.seconds = 2.0;
    selection.pacedSeconds = 3.0;
    selection.warmUp = 0.5;
    selection.threads = {1, 2, 4, 8, 16};
    selection.everyCombination = true;
    selection.pacedThreads = {1, 4, 16};
    selection.pacedBuffers = {512, kUsualBuffer};
    selection.rates = {1000, 50000};
    selection.meterThreads = {1, 4, 16};
    selection.meterWriters = {WriterKind::Udp, WriterKind::Unix};
    return selection;
}

Selection SmokeSelection()
{
    Selection selection;
    selection.seconds = 0.2;
    selection.pacedSeconds = 0.3;
    selection.warmUp = 0.1;
    selection.repeats = 1;
    selection.threads = {};
    selection.buffers = {512};
    selection.pacedThreads = {2};
    selection.pacedBuffers = {512};
    selection.rates = {2000};
    selection.meterThreads = {};
    selection.policies = {};
    selection.flushMillis = {};
    return selection;
}

Plan MakePlan(const Selection& selection)
{
    Plan plan;
    std::set<std::string> planned;
    const auto add = [&](const Scenario& scenario)
    {
        if (planned.insert(scenario.Key()).second)
        {
            plan.scenarios.push_back(scenario);
        }
    };

    for (const WriterKind writer : selection.writers)
    {
        // Whether this system sends datagrams of a buffer of that size on a socket of this kind.
        std::map<unsigned int, bool> known;
        const auto carries = [&](unsigned int buffer)
        {
            if (buffer == 0)
            {
                return true;
            }
            const auto found = known.find(buffer);
            if (found != known.end())
            {
                return found->second;
            }
            const bool fits = Receiver::Carries(writer, buffer + kLongestLine);
            known[buffer] = fits;
            if (!fits)
            {
                plan.leftOut.push_back(WriterName(writer) + " with a buffer of " + std::to_string(buffer) +
                                       " bytes: this system does not send datagrams of that size on such a socket");
            }
            return fits;
        };

        // The buffer size at which the thread counts and the meters are compared: the usual one, or else the
        // largest of the sizes below it that fits, or else the first size that fits.
        unsigned int usual = 0;
        for (const unsigned int buffer : selection.buffers)
        {
            if (buffer != 0 && buffer <= kUsualBuffer && buffer > usual && carries(buffer))
            {
                usual = buffer;
            }
        }
        for (const unsigned int buffer : selection.buffers)
        {
            if (usual == 0 && buffer != 0 && carries(buffer))
            {
                usual = buffer;
            }
        }

        // Flat out, with a counter that the thread created once and that has tags.
        for (const unsigned int buffer : selection.buffers)
        {
            if (!carries(buffer))
            {
                continue;
            }
            Scenario scenario;
            scenario.writer = writer;
            scenario.bufferSize = buffer;
            if (buffer == 0)
            {
                // Without buffering the calling thread sends, and the library does not allow a second one.
                if (!selection.threads.empty())
                {
                    add(scenario);
                }
                continue;
            }
            for (const int threads : selection.threads)
            {
                if (selection.everyCombination || buffer == usual || threads == selection.bufferThreads)
                {
                    scenario.threads = threads;
                    add(scenario);
                }
            }
        }

        // The other full-buffer policies.
        for (const Policy policy : selection.policies)
        {
            for (const int threads : selection.threads)
            {
                if (usual != 0)
                {
                    Scenario scenario;
                    scenario.writer = writer;
                    scenario.bufferSize = usual;
                    scenario.threads = threads;
                    scenario.policy = policy;
                    add(scenario);
                }
            }
        }

        // Paced.
        for (const unsigned int wanted : selection.pacedBuffers)
        {
            const unsigned int buffer = wanted == kUsualBuffer ? usual : wanted;
            if (buffer == 0 || !carries(buffer))
            {
                continue;
            }
            for (const int threads : selection.pacedThreads)
            {
                for (const int rate : selection.rates)
                {
                    Scenario scenario;
                    scenario.writer = writer;
                    scenario.bufferSize = buffer;
                    scenario.threads = threads;
                    scenario.load = Load::Paced;
                    scenario.linesPerSecond = rate;
                    add(scenario);
                    for (const int flushMillis : selection.flushMillis)
                    {
                        scenario.flushMillis = flushMillis;
                        add(scenario);
                    }
                    scenario.flushMillis = kDefaultFlushMillis;
                }
            }
        }

        // The ways to use a meter.
        const bool compareMeters = std::find(selection.meterWriters.begin(), selection.meterWriters.end(), writer) !=
                                   selection.meterWriters.end();
        if (compareMeters && usual != 0)
        {
            for (const int threads : selection.meterThreads)
            {
                for (const bool createMeter : {false, true})
                {
                    for (const bool tags : {true, false})
                    {
                        Scenario scenario;
                        scenario.writer = writer;
                        scenario.bufferSize = usual;
                        scenario.threads = threads;
                        scenario.createMeter = createMeter;
                        scenario.tags = tags;
                        add(scenario);
                    }
                }
            }
        }
    }
    return plan;
}

double MeasuredSeconds(const Scenario& scenario, const Selection& selection)
{
    return scenario.load == Load::Paced ? selection.pacedSeconds : selection.seconds;
}

double EstimatedSeconds(const Plan& plan, const Selection& selection)
{
    double seconds = 0;
    for (const Scenario& scenario : plan.scenarios)
    {
        seconds += selection.warmUp + MeasuredSeconds(scenario, selection) + kOverheadSeconds;
    }
    return seconds * selection.repeats;
}

std::string Describe(const Scenario& scenario)
{
    std::string text = WriterName(scenario.writer) + ", buffer " +
                       (scenario.bufferSize == 0 ? std::string("none") : std::to_string(scenario.bufferSize)) + ", " +
                       std::to_string(scenario.threads) + (scenario.threads == 1 ? " thread, " : " threads, ");
    text += scenario.load == Load::Paced ? "paced at " + std::to_string(scenario.linesPerSecond) + " lines/s"
                                         : std::string("flat out");
    text += scenario.createMeter ? ", counter created per line" : "";
    text += scenario.tags ? "" : ", no tags";
    text += scenario.policy == Policy::Block ? "" : ", " + PolicyName(scenario.policy) + " policy";
    text += scenario.flushMillis == kDefaultFlushMillis ? ""
                                                        : ", flush every " + std::to_string(scenario.flushMillis) + " ms";
    return text;
}

}  // namespace perf
