#include <registry.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <optional>
#include <unordered_map>
#include <string>
#include <atomic>
#include <thread>
#include <vector>

using namespace spectator;

struct RunTimeConfig
{
    bool bufferingEnabled;
    std::string writerType;
    std::string writerTypeName;
    std::string counterName;
    std::string locationTag;
};

void PrintUsage()
{
    std::cerr << "Usage: performance_test [writer_type] [buffering]" << std::endl;
    std::cerr << "  writer_type: udp or uds" << std::endl;
    std::cerr << "  buffering: 0 for disabled, 1 for enabled (default is 0)" << std::endl;
}

std::optional<RunTimeConfig> HandleArgs(int argc, char* argv[])
{
    if (argc != 3 && argc != 2)
    {
        return std::nullopt;
    }

    bool bufferingEnabled = false;
    if (argc == 3)
    {
        std::string bufferingArg = argv[2];
        if (bufferingArg != "0" && bufferingArg != "1")
        {
            std::cerr << "Invalid buffering argument: " << bufferingArg << std::endl;
            return std::nullopt;
        }
        bufferingEnabled = (bufferingArg == "1");
    }
    
    std::string writerArg = argv[1];
    if (writerArg != "udp" && writerArg != "uds")
    {
        std::cerr << "Invalid writer type: " << writerArg << std::endl;
        return std::nullopt;
    }

    RunTimeConfig config;
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
    config.bufferingEnabled = bufferingEnabled;

    return config;
}

int main(int argc, char* argv[])
{
    auto config = HandleArgs(argc, argv);
    if (config == std::nullopt)
    {
        PrintUsage();
        return 1;
    }

    std::cout << "Running performance test with the following configuration:" << std::endl;
    std::cout << "Writer Type: " << config->writerTypeName << std::endl;
    std::cout << "Buffering Enabled: " << (config->bufferingEnabled ? "Yes" : "No") << std::endl;
    
    // Configure the registry with or without buffering based on the command line argument
    auto writerConfig = WriterConfig(config->writerType);
    if (config->bufferingEnabled)
    {
        writerConfig = WriterConfig(config->writerType, 4096);
    }
    auto r = Registry(Config(writerConfig));
    std::unordered_map<std::string, std::string> tags = { 
        {"location", config->locationTag}, 
        {"version", "correct-horse-battery-staple"}
    };

    // Set maximum duration to 2 minutes
    constexpr int max_duration_seconds = 2 * 60;
    
    std::atomic<unsigned long long> iterations{0};
    std::atomic<bool> should_stop{false};

    // Non-buffered mode uses a single thread because the underlying sender is not thread-safe
    const unsigned int num_threads = config->bufferingEnabled ? 4u : 1u;
    std::cout << "Running performance test with " << num_threads << " threads..." << std::endl;

    auto thread_func = [&]()
    {
        while (!should_stop)
        {
            r.CreateCounter(config->counterName, tags).Increment();
            iterations.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i)
        threads.emplace_back(thread_func);

    auto start_time = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(max_duration_seconds));
    should_stop = true;

    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }

    double total_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    double rate_per_second = static_cast<double>(iterations) / total_elapsed;
    
    std::cout << "\nPerformance Test Summary:" << std::endl;
    std::cout << "Threads used: " << num_threads << std::endl;
    std::cout << "Iterations completed: " << iterations << std::endl;
    std::cout << "Total elapsed time: " << std::fixed << std::setprecision(2) << total_elapsed << " seconds" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(2) << rate_per_second << " iterations/second" << std::endl;
    std::cout << "Rate per thread: " << std::fixed << std::setprecision(2) << rate_per_second / num_threads << " iterations/second/thread" << std::endl;
    return 0;
}
