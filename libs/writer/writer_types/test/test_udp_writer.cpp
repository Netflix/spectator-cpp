#include <libs/writer/writer_types/include/udp_writer.h>

#include <gtest/gtest.h>

#include "../test_utils/udp_server/udp_server.h"  // Include our new header for UDP server interaction
#include <thread>
#include <chrono>
#include <algorithm>  // For std::find

// Test fixture for UDP Writer tests
class UDPWriterTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        // Set the server to run
        server_running = true;

        // Clear any existing messages from previous tests
        clear_messages();

        // Start the UDP server in a separate thread
        server_thread = std::thread(
            []()
            {
                // This calls our server function directly
                listen_for_udp_messages();
            });

        // Give the server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void TearDown() override
    {
        // Signal the server to stop
        server_running = false;

        // Terminate the server thread
        if (server_thread.joinable())
        {
            server_thread.join();
        }
    }

    std::thread server_thread;
};

TEST_F(UDPWriterTest, SendMessage)
{
    // Create a UDP writer that will connect to localhost:1234
    UDPWriter writer("127.0.0.1", 1234);

    // Define our test message
    const std::string test_message = "Hello from UDP Writer Test";

    // Send a test message
    writer.Write(test_message);

    // Give time for the message to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get current messages and verify the message was received
    const auto messages = get_messages();
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

TEST_F(UDPWriterTest, CloseAndReopen)
{
    UDPWriter writer("127.0.0.1", 1234);
    std::string message1 = "Initial message";
    writer.Write(message1);

    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify first message
    auto messages = get_messages();
    ASSERT_FALSE(messages.empty());
    ASSERT_EQ(messages.back(), message1);

    // Close the writer
    writer.Close();

    // Create a new writer
    UDPWriter writer2("127.0.0.1", 1234);
    std::string message2 = "Message after reopening";
    writer2.Write(message2);

    // Wait for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify second message
    messages = get_messages();
    ASSERT_GE(messages.size(), 2);
    ASSERT_EQ(messages.back(), message2);
}

TEST_F(UDPWriterTest, SendMultipleMessages)
{
    UDPWriter writer("127.0.0.1", 1234);

    // Define test messages
    const std::vector<std::string> test_messages = {"Message 1", "Message 2", "Message 3"};

    // Send several messages in succession
    for (const auto& msg : test_messages)
    {
        writer.Write(msg);
    }

    // Give time for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get received messages and verify
    const auto received_messages = get_messages();

    // Verify we received at least the number of messages we sent
    ASSERT_EQ(received_messages.size(), test_messages.size());

    ASSERT_EQ(test_messages.at(0), received_messages.at(0));
    ASSERT_EQ(test_messages.at(1), received_messages.at(1));
    ASSERT_EQ(test_messages.at(2), received_messages.at(2));
}