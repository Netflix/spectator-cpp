#include <udp_writer.h>

#include <udp_server.h>
#include <wait_for.h>

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;
using spectator::test::WaitFor;

// Test fixture for UDP Writer tests
class UDPWriterTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        clear_udp_messages();
        StartServer();
    }

    void TearDown() override { StopServer(); }

    void StartServer()
    {
        if (!server_thread.joinable())
        {
            udp_server_running = true;
            server_thread = std::thread([] { listen_for_udp_messages(kDefaultUdpServerPort); });
            ASSERT_TRUE(wait_for_udp_server());
        }
    }

    void StopServer()
    {
        udp_server_running = false;
        if (server_thread.joinable())
        {
            server_thread.join();
        }
    }

    std::thread server_thread;
};

TEST_F(UDPWriterTest, SendMessage)
{
    UDPWriter writer("127.0.0.1", kDefaultUdpServerPort);
    const std::string test_message = "Hello from UDP Writer Test";

    writer.Write(test_message);

    ASSERT_TRUE(WaitFor([] { return !get_udp_messages().empty(); }));
    EXPECT_EQ(get_udp_messages().back(), test_message);
}

TEST_F(UDPWriterTest, CloseAndReopen)
{
    UDPWriter writer("127.0.0.1", kDefaultUdpServerPort);
    const std::string message1 = "Initial message";
    writer.Write(message1);
    ASSERT_TRUE(WaitFor([] { return get_udp_messages().size() == 1; }));
    EXPECT_EQ(get_udp_messages().back(), message1);

    writer.Close();

    UDPWriter writer2("127.0.0.1", kDefaultUdpServerPort);
    const std::string message2 = "Message after reopening";
    writer2.Write(message2);
    ASSERT_TRUE(WaitFor([] { return get_udp_messages().size() == 2; }));
    EXPECT_EQ(get_udp_messages().back(), message2);
}

TEST_F(UDPWriterTest, SendMultipleMessages)
{
    UDPWriter writer("127.0.0.1", kDefaultUdpServerPort);
    const std::vector<std::string> test_messages = {"Message 1", "Message 2", "Message 3"};

    for (const auto& msg : test_messages)
    {
        writer.Write(msg);
    }

    // One datagram per Write, in order.
    ASSERT_TRUE(WaitFor([&] { return get_udp_messages().size() >= test_messages.size(); }));
    EXPECT_EQ(get_udp_messages(), test_messages);
}

TEST_F(UDPWriterTest, ClientReconnectsWhenServerStartsLater)
{
    StopServer();
    clear_udp_messages();

    // Nothing receives these; the writer logs any error and carries on.
    UDPWriter writer("127.0.0.1", kDefaultUdpServerPort);
    for (int i = 0; i < 3; ++i)
    {
        writer.Write("Message sent before server starts");
    }
    // Loopback delivery is asynchronous on macOS: a datagram can still be queued in the kernel when sendto returns.
    // Give the kernel time to drop these, or the server started below could bind the port first and receive them.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(get_udp_messages().empty()) << "Expected no messages when server is not running";

    StartServer();

    const std::string test_message_after = "Message sent after server starts";
    writer.Write(test_message_after);
    ASSERT_TRUE(WaitFor([] { return !get_udp_messages().empty(); }));
    EXPECT_EQ(get_udp_messages(), std::vector<std::string>{test_message_after});
}

TEST_F(UDPWriterTest, MaxPayloadSizeFitsInOneDatagram)
{
    UDPWriter writer("127.0.0.1", kDefaultUdpServerPort);
    const auto maxPayload = writer.MaxPayloadSize();
    ASSERT_GT(maxPayload, 0u);
    ASSERT_LE(maxPayload, 65507u);

    // The limit comes from the socket's send buffer (9216 bytes on macOS by default), so a payload of exactly that
    // size must arrive whole.
    const std::string payload(maxPayload, 'x');
    writer.Write(payload);
    ASSERT_TRUE(WaitFor([] { return !get_udp_messages().empty(); }));
    EXPECT_EQ(get_udp_messages().back().size(), maxPayload);
}
