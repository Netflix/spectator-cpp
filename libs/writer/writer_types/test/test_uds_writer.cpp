#include <uds_writer.h>

#include <gtest/gtest.h>
#include "../test_utils/uds_server/uds_server.h"
#include <boost/asio.hpp>
#include <boost/asio/local/datagram_protocol.hpp>
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>

using namespace spectator;

#if defined(__linux__)
// Linux makes a sender wait in the kernel for as long as the queue of the receiver's socket is full.
constexpr bool kSendToFullQueueFails = false;
#else
// Elsewhere, as on macOS, such a send fails with "no buffer space".
constexpr bool kSendToFullQueueFails = true;
#endif

// Test fixture for UDS Writer tests
class UDSWriterTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        // Clear any existing messages from previous tests
        clear_uds_messages();
        
        // By default, start the server for most tests
        StartServer();
    }

    void TearDown() override
    {
        StopServer();
    }

    void StartServer()
    {
        if (!server_thread.joinable()) // Only start if not already running
        {
            // Set the server to run
            uds_server_running = true;

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
    }

    void StopServer()
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

TEST_F(UDSWriterTest, SendMessage)
{
    // Create a UDS writer that will connect to the test socket
    UDSWriter writer("/tmp/test_uds_socket");

    // Define our test message
    const std::string test_message = "Hello from UDS Writer Test";

    // Send a test message
    writer.Write(test_message);

    // Give time for the message to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get current messages and verify the message was received
    const auto messages = get_uds_messages();
    ASSERT_FALSE(messages.empty());

    // Check that our message is in the vector
    bool message_found = false;
    for (const auto& msg : messages)
    {
        if (msg == test_message)
        {
            message_found = true;
            break;
        }
    }

    ASSERT_TRUE(message_found);
}

TEST_F(UDSWriterTest, CloseAndReopen)
{
    UDSWriter writer("/tmp/test_uds_socket");
    const std::string message1 = "Initial message";
    writer.Write(message1);

    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify first message
    auto messages = get_uds_messages();
    ASSERT_FALSE(messages.empty());
    ASSERT_EQ(messages.back(), message1);

    // Close the writer
    writer.Close();

    // Create a new writer
    UDSWriter writer2("/tmp/test_uds_socket");
    const std::string message2 = "Message after reopening";
    writer2.Write(message2);

    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify second message
    messages = get_uds_messages();
    ASSERT_GE(messages.size(), 2);
    ASSERT_EQ(messages.back(), message2);
}

TEST_F(UDSWriterTest, SendMultipleMessages)
{
    UDSWriter writer("/tmp/test_uds_socket");

    // Define test messages
    const std::vector<std::string> test_messages = {"Message 1", "Message 2", "Message 3"};

    // Send messages one by one, with a separate connection for each
    for (const auto& msg : test_messages)
    {
        writer.Write(msg);
        // Wait for the message to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Get received messages and verify
    const auto received_messages = get_uds_messages();

    // Verify we received the number of messages we sent
    ASSERT_EQ(received_messages.size(), test_messages.size());

    // Verify each message was received correctly
    for (size_t i = 0; i < test_messages.size(); i++)
    {
        ASSERT_EQ(test_messages.at(i), received_messages.at(i));
    }
}

TEST_F(UDSWriterTest, ClientReconnectsWhenServerStartsLater)
{
    // First, stop the server that was started in SetUp
    StopServer();
    
    // Clear any existing messages
    clear_uds_messages();
    
    // Create a client before the server is available
    UDSWriter writer("/tmp/test_uds_socket");
    
    // Try to send a message while server is not running
    // This should fail silently (UDSWriter logs errors but doesn't throw)
    for (int i = 0; i < 3; ++i)
    {
        const std::string test_message_before = "Message sent before server starts";
        writer.Write(test_message_before);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Verify no messages were received (server wasn't running)
    auto messages = get_uds_messages();
    EXPECT_TRUE(messages.empty()) << "Expected no messages when server is not running";
    
    // Now start the server
    StartServer();
    
    // Try to send a message after server is started
    // The UDSWriter should automatically reconnect on the next Write call
    const std::string test_message_after = "Message sent after server starts";
    writer.Write(test_message_after);
    
    // Give time for the message to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify the message was received
    messages = get_uds_messages();
    EXPECT_TRUE(messages.size() == 1);
    EXPECT_TRUE(messages.at(0) == test_message_after);
}

// A burst of messages can fill the queue of the receiver's socket before the server gets to read: the server looks
// for messages every 10 ms, and the queue may hold as few as ten of them (32 on macOS by default). With a
// receiverFullWait, the burst still arrives in full. Linux makes the sender wait while the queue is full. Where the
// send fails instead, as on macOS, the writer tries again for up to receiverFullWait.
TEST_F(UDSWriterTest, DeliversABurstToAReceiverThatIsSlowToRead)
{
    UDSWriter writer("/tmp/test_uds_socket", std::chrono::seconds(10));

    constexpr size_t burst = 64;
    for (size_t i = 0; i < burst; i++)
    {
        writer.Write("Burst message " + std::to_string(i));
    }

    // Give the server time to read them all
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (get_uds_messages().size() < burst && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto messages = get_uds_messages();
    ASSERT_EQ(messages.size(), burst);
    for (size_t i = 0; i < burst; i++)
    {
        EXPECT_EQ(messages.at(i), "Burst message " + std::to_string(i));
    }
}

// The writer waits for a receiver whose queue is full, but only for receiverFullWait per message. Then it gives the
// message up, so that a receiver that has stopped reading does not stop the sender for good.
TEST(UDSWriterReceiverFullTest, GivesUpOnAReceiverThatDoesNotRead)
{
    if (!kSendToFullQueueFails)
    {
        GTEST_SKIP() << "On this platform a send to a full queue waits in the kernel instead of failing";
    }

    // A receiver that does not read
    using DatagramProtocol = boost::asio::local::datagram_protocol;
    const std::string socketPath = "/tmp/test_uds_full_socket";
    std::filesystem::remove(socketPath);
    boost::asio::io_context ioContext;
    DatagramProtocol::socket receiver(ioContext, DatagramProtocol::endpoint(socketPath));
    receiver.non_blocking(true);

    std::vector<std::string> received;
    const auto readQueue = [&receiver, &received]
    {
        std::array<char, 2048> buffer{};
        for (;;)
        {
            boost::system::error_code ec;
            const auto bytesRead = receiver.receive(boost::asio::buffer(buffer), 0, ec);
            if (ec)
            {
                return;
            }
            received.emplace_back(buffer.data(), bytesRead);
        }
    };

    // Write until three messages have each taken as long as the writer waits for a full queue. This runs on a thread
    // of its own, so that a writer that does not give up fails the test instead of hanging it.
    struct Written
    {
        size_t messages = 0;
        std::chrono::milliseconds elapsed{0};
    };
    const auto receiverFullWait = std::chrono::milliseconds(50);
    std::future<Written> writing = std::async(
        std::launch::async,
        [&socketPath, receiverFullWait]
        {
            constexpr size_t mostMessages = 10000;
            UDSWriter writer(socketPath, receiverFullWait);
            Written written;
            int slowWrites = 0;
            const auto start = std::chrono::steady_clock::now();
            while (written.messages < mostMessages && slowWrites < 3)
            {
                const auto before = std::chrono::steady_clock::now();
                writer.Write("Message " + std::to_string(written.messages));
                written.messages++;
                if (std::chrono::steady_clock::now() - before >= receiverFullWait)
                {
                    slowWrites++;
                }
            }
            written.elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            return written;
        });

    const bool gaveUp = writing.wait_for(std::chrono::seconds(30)) == std::future_status::ready;
    EXPECT_TRUE(gaveUp) << "The writer is still waiting for the receiver";
    while (writing.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
    {
        // The writer did not give up. Reading makes room in the queue, so that it gets to the end.
        readQueue();
    }
    const Written written = writing.get();
    if (gaveUp)
    {
        readQueue();
    }
    std::filesystem::remove(socketPath);

    if (!gaveUp)
    {
        return;
    }
    if (received.size() == written.messages)
    {
        GTEST_SKIP() << "The queue of the receiver did not fill up with " << written.messages << " messages";
    }

    // The queue holds the messages from before it was full, in order. The writer gave the others up, each after it
    // had waited for receiverFullWait.
    ASSERT_LT(received.size(), written.messages);
    for (size_t i = 0; i < received.size(); i++)
    {
        EXPECT_EQ(received.at(i), "Message " + std::to_string(i));
    }
    const auto givenUp = static_cast<long long>(written.messages - received.size());
    EXPECT_GE(written.elapsed.count(), givenUp * receiverFullWait.count());
}
