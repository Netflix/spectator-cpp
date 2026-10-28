#include <uds_writer.h>

#include <uds_server.h>
#include <wait_for.h>

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;
using spectator::test::WaitFor;

namespace {
// Unique to this binary, so it can run alongside other tests that start a UDS server.
const std::string kSocketPath = "/tmp/spectator_test_uds_writer.sock";
}  // namespace

// Test fixture for UDS Writer tests
class UDSWriterTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        clear_uds_messages();
        StartServer();
    }

    void TearDown() override { StopServer(); }

    void StartServer()
    {
        if (!server_thread.joinable())
        {
            uds_server_running = true;
            server_thread = std::thread([] { listen_for_uds_messages(kSocketPath); });
            ASSERT_TRUE(wait_for_uds_server());
        }
    }

    void StopServer()
    {
        uds_server_running = false;
        if (server_thread.joinable())
        {
            server_thread.join();
        }
    }

    std::thread server_thread;
};

TEST_F(UDSWriterTest, SendMessage)
{
    UDSWriter writer(kSocketPath);
    const std::string test_message = "Hello from UDS Writer Test";

    writer.Write(test_message);

    ASSERT_TRUE(WaitFor([] { return !get_uds_messages().empty(); }));
    EXPECT_EQ(get_uds_messages().back(), test_message);
}

TEST_F(UDSWriterTest, CloseAndReopen)
{
    UDSWriter writer(kSocketPath);
    const std::string message1 = "Initial message";
    writer.Write(message1);
    ASSERT_TRUE(WaitFor([] { return get_uds_messages().size() == 1; }));
    EXPECT_EQ(get_uds_messages().back(), message1);

    writer.Close();

    UDSWriter writer2(kSocketPath);
    const std::string message2 = "Message after reopening";
    writer2.Write(message2);
    ASSERT_TRUE(WaitFor([] { return get_uds_messages().size() == 2; }));
    EXPECT_EQ(get_uds_messages().back(), message2);
}

TEST_F(UDSWriterTest, SendMultipleMessages)
{
    UDSWriter writer(kSocketPath);
    const std::vector<std::string> test_messages = {"Message 1", "Message 2", "Message 3"};

    for (const auto& msg : test_messages)
    {
        writer.Write(msg);
    }

    // One datagram per Write, in order.
    ASSERT_TRUE(WaitFor([&] { return get_uds_messages().size() >= test_messages.size(); }));
    EXPECT_EQ(get_uds_messages(), test_messages);
}

TEST_F(UDSWriterTest, ClientReconnectsWhenServerStartsLater)
{
    StopServer();
    clear_uds_messages();

    // Writes fail while nothing listens on the socket path; the writer logs and carries on.
    UDSWriter writer(kSocketPath);
    for (int i = 0; i < 3; ++i)
    {
        writer.Write("Message sent before server starts");
    }
    EXPECT_TRUE(get_uds_messages().empty()) << "Expected no messages when server is not running";

    StartServer();

    // The next write reconnects.
    const std::string test_message_after = "Message sent after server starts";
    writer.Write(test_message_after);
    ASSERT_TRUE(WaitFor([] { return !get_uds_messages().empty(); }));
    EXPECT_EQ(get_uds_messages(), std::vector<std::string>{test_message_after});
}

TEST_F(UDSWriterTest, MaxPayloadSizeFitsInOneDatagram)
{
    UDSWriter writer(kSocketPath);
    const auto maxPayload = writer.MaxPayloadSize();
    ASSERT_GT(maxPayload, 0u);
    ASSERT_LE(maxPayload, 65507u);

    // The limit comes from the socket's send buffer (2048 bytes on macOS by default), so a payload of exactly that
    // size must arrive whole.
    const std::string payload(maxPayload, 'x');
    writer.Write(payload);
    ASSERT_TRUE(WaitFor([] { return !get_uds_messages().empty(); }));
    EXPECT_EQ(get_uds_messages().back().size(), maxPayload);
}
