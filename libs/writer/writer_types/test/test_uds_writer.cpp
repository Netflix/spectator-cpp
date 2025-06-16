
#include <libs/writer/writer_types/include/uds_writer.h>

#include <gtest/gtest.h>
#include "../test_utils/uds_server/uds_server.h"
#include <thread>
#include <chrono>
#include <algorithm>

// Test fixture for UDS Writer tests
class UDSWriterTest : public ::testing::Test
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
            []()
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

TEST_F(UDSWriterTest, SendMessage)
{
    // Create a UDS writer that will connect to the test socket
    UDSWriter writer("/tmp/test_uds_socket");

    // Define our test message
    std::string test_message = "Hello from UDS Writer Test";

    // Send a test message
    writer.Write(test_message);

    // Give time for the message to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get current messages and verify the message was received
    auto messages = get_uds_messages();
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
    std::string message1 = "Initial message";
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
    std::string message2 = "Message after reopening";
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
    std::vector<std::string> test_messages = {"Message 1", "Message 2", "Message 3"};

    // Send messages one by one, with a separate connection for each
    for (const auto& msg : test_messages)
    {
        writer.Write(msg);
        // Wait for the message to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Get received messages and verify
    auto received_messages = get_uds_messages();

    // Verify we received the number of messages we sent
    ASSERT_EQ(received_messages.size(), test_messages.size());

    // Verify each message was received correctly
    for (size_t i = 0; i < test_messages.size(); i++)
    {
        ASSERT_EQ(test_messages.at(i), received_messages.at(i));
    }
}