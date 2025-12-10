#include <counter.h>
#include <logger.h>
#include <meter_id.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>
#include <fmt/core.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

#include "../writer_types/test_utils/uds_server/uds_server.h"

// Test fixture for UDS Writer tests
class WriterWrapperUDSWriterTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        // Set the server to run
        uds_server_running = true;

        // Clear any existing messages from previous tests
        clear_uds_messages();

        // Start the UDS server in a separate thread
        server_thread = std::thread(
            []
            {
                // This calls our server function directly
                listen_for_uds_messages();
            });

        // Give the server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void TearDown() override
    {
        // Signal the server to stop
        uds_server_running = false;

        // Terminate the server thread
        if (server_thread.joinable())
        {
            server_thread.join();
        }
    }

    std::thread server_thread;
};

TEST_F(WriterWrapperUDSWriterTest, MultithreadedWrite)
{
    Logger::info("Starting multithreaded write test...");

    // Create a UDS writer with a small buffer size
    const std::string unixUrl = "/tmp/test_uds_socket";
    WriterTestHelper::InitializeWriter(WriterType::Unix, unixUrl, 0, 30);

    // Number of threads and counters to create
    constexpr auto numThreads = 4;
    constexpr auto countersPerThread = 3;
    constexpr auto  incrementsPerCounter = 5;

    // Function for worker threads
    auto worker = [&](int threadId)
    {
        // Create several counters per thread with unique names
        for (int i = 0; i < countersPerThread; i++)
        {
            std::string counterName = fmt::format("counter.thread{}.{}", threadId, i);
            MeterId meterId(counterName);
            Counter counter(meterId);

            // Increment each counter multiple times
            for (int j = 0; j < incrementsPerCounter; j++)
            {
                counter.Increment();
            }
        }
    };

    // Start worker threads
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++)
    {
        threads.emplace_back(worker, i);
    }

    // Wait for all threads to complete
    for (auto& t : threads)
    {
        t.join();
    }

    // Give some time for messages to be sent
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    // Check messages
    auto msgs = get_uds_messages();
    EXPECT_FALSE(msgs.empty());

    // Verify total number of increments
    int expectedIncrements = numThreads * countersPerThread * incrementsPerCounter;
    int actualIncrements = 0;

    // Verify every string in msgs follows the form counter.thread<digit>.<digit>
    std::regex counter_regex(R"(c:counter\.thread\d+\.\d+:1.000000)");
    for (const auto& msg : msgs)
    {
        std::stringstream ss(msg);
        std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty())
            {
                EXPECT_TRUE(std::regex_match(line, counter_regex)) << "Unexpected counter format: " << line;
                actualIncrements++;
            }
        }
    }

    EXPECT_EQ(actualIncrements, expectedIncrements);
}
