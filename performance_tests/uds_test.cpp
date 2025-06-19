#include <spectator/registry.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[])
{
    if (argc > 2)
    {
        std::cerr << "Too many arguments provided." << std::endl;
        return 1;
    }

    bool bufferingEnabled = false;
    
    // If argument is provided and equals 1, enable buffering
    if (argc == 2) {
        if (std::string(argv[1]) == "1") {
            bufferingEnabled = true;
        } else {
            std::cerr << "Usage: " << argv[0] << " [1]" << std::endl;
            std::cerr << "  No argument: Run without buffering" << std::endl;
            std::cerr << "  1: Run with buffering enabled" << std::endl;
            return 1;
        }
    }
    // No arguments means bufferingEnabled remains false
    Logger::info("Starting UDP performance test with buffering {}", bufferingEnabled ? "enabled" : "disabled");
    
    // Configure the registry with or without buffering based on the command line argument
    auto writerConfig = WriterConfig(WriterTypes::Unix);
    if (bufferingEnabled)
    {
        writerConfig = WriterConfig(WriterTypes::Unix, 4096);
    }
    auto r = Registry(Config(writerConfig));
    std::unordered_map<std::string, std::string> tags = { {"location", "unix"}, {"version", "correct-horse-battery-staple"}};

    // Set maximum duration to 2 minutes
    constexpr int max_duration_seconds = 2 * 60;
    
    // Track iterations and timing
    unsigned long long iterations = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    // Helper function to get elapsed time in seconds
    auto elapsed = [&start_time]() -> double {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    };
        
    while (true)
    {
        r.counter("unix_test_counter", tags).Increment();
        iterations++;
        
        if (iterations % 500000 == 0)
        {
            if (elapsed() > max_duration_seconds)
            {
                break;
            }
        }
    }
    
    double total_elapsed = elapsed();
    double rate_per_second = iterations / total_elapsed;
    
    std::cout << "Buffering enabled: " << (bufferingEnabled ? "Yes" : "No") << std::endl;
    std::cout << "Iterations completed: " << iterations << std::endl;
    std::cout << "Total elapsed time: " << std::fixed << std::setprecision(2) << total_elapsed << " seconds" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(2) << rate_per_second << " iterations/second" << std::endl;
    return 0;
}