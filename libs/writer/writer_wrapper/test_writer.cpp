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

using namespace spectator;

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

// Verify that metrics written infrequently are still flushed via the interval mechanism
// even when the buffer is not full.
TEST_F(WriterWrapperUDSWriterTest, IntervalFlushWithIdleThread)
{
    Logger::info("Starting interval flush test...");

    // Use a large buffer so capacity-based flush never triggers,
    // but set a short flush interval so metrics get sent in time.
    const std::string unixUrl = "/tmp/test_uds_socket";
    constexpr unsigned int largeBuffer = 4096;
    constexpr unsigned int flushIntervalMs = 200;
    WriterTestHelper::InitializeWriter(WriterType::Unix, unixUrl, 0, largeBuffer, flushIntervalMs);

    MeterId meterId("interval.test.counter");
    Counter counter(meterId);
    counter.Increment();

    // Wait for at least two flush intervals to pass
    std::this_thread::sleep_for(std::chrono::milliseconds(flushIntervalMs * 3));

    auto msgs = get_uds_messages();
    EXPECT_FALSE(msgs.empty()) << "Expected metrics to be flushed by interval timer";

    std::regex counter_regex(R"(c:interval\.test\.counter:1.000000)");
    bool found = false;
    for (const auto& msg : msgs)
    {
        std::stringstream ss(msg);
        std::string line;
        while (std::getline(ss, line))
        {
            if (std::regex_match(line, counter_regex))
            {
                found = true;
            }
        }
    }
    EXPECT_TRUE(found) << "Expected interval.test.counter metric to be received";
}

// Verify that multiple worker threads do not block each other: each thread uses its own
// local buffer, flushing by capacity. The interval timer catches any tail data that
// does not fill the buffer before the threads exit.
TEST_F(WriterWrapperUDSWriterTest, ThreadLocalBufferNoMutexContention)
{
    Logger::info("Starting thread-local buffer contention test...");

    const std::string unixUrl = "/tmp/test_uds_socket";
    // Small buffer so capacity-based flush fires frequently; interval timer catches the tail
    WriterTestHelper::InitializeWriter(WriterType::Unix, unixUrl, 0, 64, 100);

    constexpr auto numThreads = 8;
    constexpr auto incrementsPerThread = 20;

    auto worker = [&](int threadId)
    {
        std::string name = fmt::format("tl.counter.{}", threadId);
        MeterId meterId(name);
        Counter counter(meterId);
        for (int j = 0; j < incrementsPerThread; j++)
        {
            counter.Increment();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++)
    {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads)
    {
        t.join();
    }

    // Allow the interval timer to flush any remaining buffered data
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto msgs = get_uds_messages();
    EXPECT_FALSE(msgs.empty());

    int totalIncrements = 0;
    std::regex counter_regex(R"(c:tl\.counter\.\d+:1.000000)");
    for (const auto& msg : msgs)
    {
        std::stringstream ss(msg);
        std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty())
            {
                EXPECT_TRUE(std::regex_match(line, counter_regex))
                    << "Unexpected line: " << line;
                totalIncrements++;
            }
        }
    }
    EXPECT_EQ(totalIncrements, numThreads * incrementsPerThread);
}

// This is a unique test that attempts to create messages of exactly 10 bytes in size
// and writes to a buffer of size 10 bytes from multiple threads. The NDrive team discovered
// a deadlock scenario in this specific case where the buffer size matched the message size
// and multiple threads were writing simultaneously. This test is designed to reproduce
// that scenario to ensure it has been resolved. Due to singleton issues this test is currently
// commented out.
/*
TEST_F(WriterWrapperUDSWriterTest, TenThreadsBufferSize10Messages)
{
    Logger::info("Starting 10 threads with buffer size 10 test...");

    // Create a UDS writer with buffer size of 10
    const std::string unixUrl = "/tmp/test_uds_socket";
    WriterTestHelper::InitializeWriter(WriterType::Unix, unixUrl, 0, 21);

    // Number of threads and counters to create
    constexpr auto numThreads = 10;
    constexpr auto countersPerThread = 1;
    constexpr auto incrementsPerCounter = 10;

    // Function for worker threads - creates counter names of size 10
    auto worker = [&](int threadId)
    {
        // Create counters with names that result in messages of size 10
        // Format: "ctr<6-digit-padded-id>" to ensure consistent message size
        for (int i = 0; i < countersPerThread; i++)
        {
            std::string counterName = fmt::format("ctr{:06d}", threadId * countersPerThread + i);
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

    for (const auto& msg : msgs)
    {
        EXPECT_EQ(msg.size(), 21);
    }
}
*/