#include <counter.h>
#include <logger.h>
#include <meter_id.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>
#include <fmt/core.h>
#include <boost/asio.hpp>
#include <boost/asio/local/datagram_protocol.hpp>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <chrono>
#include <regex>
#include <algorithm>

#include "../writer_types/test_utils/uds_server/uds_server.h"

using namespace spectator;

// A Unix datagram socket that a test reads itself. A datagram is in the queue of the socket as soon as the send has
// returned, so a test finds what was sent without waiting for it.
class DatagramReceiver
{
   public:
    explicit DatagramReceiver(const std::string& socketPath) : m_socketPath(socketPath), m_socket(m_ioContext)
    {
        std::filesystem::remove(m_socketPath);
        m_socket.open();
        m_socket.bind(DatagramProtocol::endpoint(m_socketPath));
        m_socket.non_blocking(true);
    }

    ~DatagramReceiver()
    {
        std::error_code ignored;
        std::filesystem::remove(m_socketPath, ignored);
    }

    // The next datagram in the queue, or nothing if the queue is empty.
    std::optional<std::string> Receive()
    {
        std::array<char, 4096> buffer{};
        boost::system::error_code ec;
        const auto bytesRead = m_socket.receive(boost::asio::buffer(buffer), 0, ec);
        if (ec)
        {
            return std::nullopt;
        }
        return std::string(buffer.data(), bytesRead);
    }

   private:
    using DatagramProtocol = boost::asio::local::datagram_protocol;

    std::string m_socketPath;
    boost::asio::io_context m_ioContext;
    DatagramProtocol::socket m_socket;
};

// Runs in the death test child: records one meter with a buffer far larger than the line, then exits normally,
// which destroys the Writer singleton.
static void RecordOneCounterAndExit(const std::string& socketPath)
{
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0, 4096);
    Counter counter(MeterId("shutdown.counter"));
    counter.Increment();
    std::exit(0);
}

// A line stays in the buffer of its thread until the buffer is full or the flush interval has passed, and a
// short-lived process may exit before either. The Writer is a process-wide singleton, so the child process owns it
// here. Death test suites run before all other tests, so the parent has not initialized the Writer or started any
// threads when it forks.
TEST(WriterShutdownDeathTest, SendsBufferedLinesAtExit)
{
    using DatagramProtocol = boost::asio::local::datagram_protocol;
    const std::string socketPath = "/tmp/test_uds_shutdown_socket";
    std::filesystem::remove(socketPath);
    boost::asio::io_context ioContext;
    DatagramProtocol::socket socket(ioContext, DatagramProtocol::endpoint(socketPath));
    socket.non_blocking(true);

    EXPECT_EXIT(RecordOneCounterAndExit(socketPath), testing::ExitedWithCode(0), "");

    std::array<char, 4096> buffer{};
    boost::system::error_code ec;
    const auto bytesRead = socket.receive(boost::asio::buffer(buffer), 0, ec);
    std::filesystem::remove(socketPath);
    ASSERT_FALSE(ec) << "nothing was sent at exit: " << ec.message();
    EXPECT_EQ(std::string(buffer.data(), bytesRead), "c:shutdown.counter:1.000000\n");
}

// Runs in the death test child: another thread records one meter and is still running when the process exits
// normally.
static void RecordOnAnotherThreadAndExit(const std::string& socketPath)
{
    // A flush interval that is longer than the test, so the line is still in the buffer of its thread at exit.
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0, 4096, std::chrono::hours(1));
    std::promise<void> recorded;
    std::thread(
        [&recorded]
        {
            Counter counter(MeterId("running.thread.counter"));
            counter.Increment();
            recorded.set_value();
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::hours(1));
            }
        })
        .detach();
    recorded.get_future().wait();
    std::exit(0);
}

// A thread that is still running when the process exits has not handed its buffer over. The Writer sends it.
TEST(WriterShutdownDeathTest, SendsTheLinesOfRunningThreadsAtExit)
{
    const std::string socketPath = "/tmp/test_uds_shutdown_threads_socket";
    DatagramReceiver receiver(socketPath);

    EXPECT_EXIT(RecordOnAnotherThreadAndExit(socketPath), testing::ExitedWithCode(0), "");

    EXPECT_EQ(receiver.Receive(), "c:running.thread.counter:1.000000\n");
    EXPECT_EQ(receiver.Receive(), std::nullopt);
}

// Runs in the death test child: threads record without pause while the process exits normally.
static void ExitWhileThreadsAreRecording()
{
    WriterTestHelper::InitializeWriter(WriterType::Memory, "", 0, 64);
    for (int i = 0; i < 4; i++)
    {
        std::thread(
            [i]
            {
                Counter counter(MeterId(fmt::format("exit.counter.{}", i)));
                for (;;)
                {
                    counter.Increment();
                }
            })
            .detach();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::exit(0);
}

// The Writer is destroyed while threads are recording. A thread that waits for buffer space is released, and what
// the threads record from then on is dropped: none of it may crash the process or keep it from exiting.
TEST(WriterShutdownDeathTest, ExitsCleanlyWhileThreadsAreRecording)
{
    EXPECT_EXIT(ExitWhileThreadsAreRecording(), testing::ExitedWithCode(0), "");
}

// The Writer can be initialized again, which replaces the socket writer. A buffered writer from the earlier
// initialization first sends what it still holds.
TEST(WriterReinitializeTest, SendsWhatThePreviousWriterBuffered)
{
    const std::string socketPath = "/tmp/test_uds_reinitialize_socket";
    DatagramReceiver receiver(socketPath);
    // Longer than the test, so nothing is sent on a timer.
    const auto noTimedFlush = std::chrono::hours(1);

    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0, 4096, noTimedFlush);
    Counter counter(MeterId("reinitialize.counter"));
    counter.Increment();
    EXPECT_EQ(receiver.Receive(), std::nullopt);

    // Buffered again: the first writer sends the line before it is replaced.
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0, 4096, noTimedFlush);
    EXPECT_EQ(receiver.Receive(), "c:reinitialize.counter:1.000000\n");
    counter.Increment(2);
    EXPECT_EQ(receiver.Receive(), std::nullopt);

    // Without buffering: the second writer sends its line, and from then on every line is sent as it is recorded.
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath);
    EXPECT_EQ(receiver.Receive(), "c:reinitialize.counter:2.000000\n");
    counter.Increment(3);
    EXPECT_EQ(receiver.Receive(), "c:reinitialize.counter:3.000000\n");
    EXPECT_EQ(receiver.Receive(), std::nullopt);
}

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