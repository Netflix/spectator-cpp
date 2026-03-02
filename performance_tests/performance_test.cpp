#include <logger.h>
#include <registry.h>
#include <write_mode.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <optional>
#include <unordered_map>
#include <string>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <vector>

using namespace spectator;

struct RunTimeConfig
{
    WriteModeType modeType;
    std::string writerType;
    std::string writerTypeName;
    std::string counterName;
    std::string locationTag;
    std::string modeName;
    unsigned int numThreads = 1;
    unsigned int bufferSize = 4096;
};

struct PerfResults
{
    unsigned long long iterations;
    double elapsedSeconds;
    unsigned int numThreads;
    std::string modeName;
};

void PrintUsage()
{
    std::cerr << "Usage: performance_test [writer_type] [write_mode] [num_threads] [buffer_size]" << std::endl;
    std::cerr << "  writer_type: udp or uds" << std::endl;
    std::cerr << "  write_mode: 0 for non-buffered, 1 for buffered, 2 for thread-local (default is 0)" << std::endl;
    std::cerr << "  num_threads: number of producer threads (default is 1)" << std::endl;
    std::cerr << "  buffer_size: buffer size in bytes for buffered modes (default is 4096)" << std::endl;
}

std::optional<WriteModeType> ParseWriteMode(const std::string& arg)
{
    if (arg == "0") return WriteModeType::NonBuffered;
    if (arg == "1") return WriteModeType::Buffered;
    if (arg == "2") return WriteModeType::ThreadLocalBuffered;
    return std::nullopt;
}

std::string WriteModeToString(WriteModeType mode)
{
    switch (mode)
    {
        case WriteModeType::NonBuffered: return "NonBuffered";
        case WriteModeType::Buffered: return "Buffered";
        case WriteModeType::ThreadLocalBuffered: return "ThreadLocalBuffered";
        default: return "Auto";
    }
}

std::optional<RunTimeConfig> HandleArgs(int argc, char* argv[])
{
    if (argc < 2 || argc > 5)
    {
        return std::nullopt;
    }

    std::string writerArg = argv[1];
    if (writerArg != "udp" && writerArg != "uds")
    {
        std::cerr << "Invalid writer type: " << writerArg << std::endl;
        return std::nullopt;
    }

    WriteModeType modeType = WriteModeType::NonBuffered;
    if (argc >= 3)
    {
        auto parsed = ParseWriteMode(argv[2]);
        if (!parsed)
        {
            std::cerr << "Invalid write mode argument: " << argv[2] << std::endl;
            return std::nullopt;
        }
        modeType = *parsed;
    }

    unsigned int numThreads = 1;
    if (argc >= 4)
    {
        int parsed = std::atoi(argv[3]);
        if (parsed <= 0)
        {
            std::cerr << "Invalid num_threads argument: " << argv[3] << std::endl;
            return std::nullopt;
        }
        numThreads = static_cast<unsigned int>(parsed);
    }

    unsigned int bufferSize = 4096;
    if (argc == 5)
    {
        int parsed = std::atoi(argv[4]);
        if (parsed <= 0)
        {
            std::cerr << "Invalid buffer_size argument: " << argv[4] << std::endl;
            return std::nullopt;
        }
        bufferSize = static_cast<unsigned int>(parsed);
    }

    RunTimeConfig config;
    config.modeType = modeType;
    config.modeName = WriteModeToString(modeType);
    config.numThreads = numThreads;
    config.bufferSize = bufferSize;

    if (writerArg == "udp")
    {
        config.writerType = WriterTypes::UDP;
        config.counterName = "udp_test_counter";
        config.locationTag = "udp";
        config.writerTypeName = "UDP";
    }
    else
    {
        config.writerType = WriterTypes::Unix;
        config.counterName = "unix_test_counter";
        config.locationTag = "unix";
        config.writerTypeName = "UDS";
    }

    return config;
}

Registry CreateRegistry(const RunTimeConfig& config)
{
    WriterConfig writerConfig(config.writerType);
    if (config.modeType != WriteModeType::NonBuffered)
    {
        writerConfig = WriterConfig(config.writerType, config.bufferSize, config.modeType);
    }
    return Registry(Config(writerConfig));
}

PerfResults RunBenchmark(Registry& registry, const RunTimeConfig& config)
{
    constexpr int benchmarkSeconds = 2 * 60;
    std::unordered_map<std::string, std::string> tags = {
        {"location", config.locationTag},
        {"version", "correct-horse-battery-staple"}
    };

    std::atomic<unsigned long long> iterations{0};
    std::atomic<bool> shouldStop{false};

    auto threadFunc = [&registry, &config, &tags, &iterations, &shouldStop]()
    {
        while (!shouldStop.load())
        {
            registry.CreateCounter(config.counterName, tags).Increment();
            iterations.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < config.numThreads; ++i)
    {
        threads.emplace_back(threadFunc);
    }

    auto startTime = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(benchmarkSeconds));
    shouldStop.store(true);

    for (auto& t : threads)
    {
        t.join();
    }

    auto totalElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    return {iterations.load(), totalElapsed, config.numThreads, config.modeName};
}

void PrintResults(const PerfResults& results)
{
    double ratePerSecond = static_cast<double>(results.iterations) / results.elapsedSeconds;

    std::cout << "\nPerformance Test Summary:" << std::endl;
    std::cout << "Threads used: " << results.numThreads << std::endl;
    std::cout << "Write Mode: " << results.modeName << std::endl;
    std::cout << "Iterations completed: " << results.iterations << std::endl;
    std::cout << "Total elapsed time: " << std::fixed << std::setprecision(2) << results.elapsedSeconds << " seconds" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(2) << ratePerSecond << " iterations/second" << std::endl;
    std::cout << "Rate per thread: " << std::fixed << std::setprecision(2) << ratePerSecond / results.numThreads << " iterations/second/thread" << std::endl;
}

int main(int argc, char* argv[])
{
    //Logger::GetLogger()->set_level(spdlog::level::critical);

    auto config = HandleArgs(argc, argv);
    if (!config)
    {
        PrintUsage();
        return 1;
    }

    std::cout << "Running performance test with the following configuration:" << std::endl;
    std::cout << "Writer Type: " << config->writerTypeName << std::endl;
    std::cout << "Write Mode: " << config->modeName << std::endl;
    std::cout << "Threads: " << config->numThreads << std::endl;
    if (config->modeType != WriteModeType::NonBuffered)
    {
        std::cout << "Buffer Size: " << config->bufferSize << " bytes" << std::endl;
    }

    auto registry = CreateRegistry(*config);
    auto results = RunBenchmark(registry, *config);
    PrintResults(results);

    return 0;
}
