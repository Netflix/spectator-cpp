#include <counter.h>
#include <meter_id.h>
#include <writer_test_helper.h>

#include <recording_writer.h>
#include <udp_server.h>
#include <uds_server.h>
#include <wait_for.h>

#include <gtest/gtest.h>
#include <fmt/core.h>
#include <boost/asio.hpp>
#include <boost/asio/local/datagram_protocol.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;
using namespace std::chrono_literals;
using spectator::test::Recording;
using spectator::test::RecordingWriter;
using spectator::test::WaitFor;
using Lines = std::vector<std::string>;

namespace {

// Unique to this binary, so it can run alongside the other writer tests.
const std::string kServerSocketPath = "/tmp/spectator_test_writer.sock";
constexpr int kUdpServerPort = 12346;

Lines SplitLines(const std::vector<std::string>& datagrams)
{
    Lines lines;
    for (const auto& datagram : datagrams)
    {
        std::istringstream stream(datagram);
        for (std::string line; std::getline(stream, line);)
        {
            lines.push_back(line);
        }
    }
    return lines;
}

// A Unix datagram socket in the test process. Death tests read from it what the child sent before it exited.
class DatagramReceiver
{
   public:
    explicit DatagramReceiver(std::string path) : m_path(std::move(path)), m_socket(m_ioContext)
    {
        std::filesystem::remove(m_path);
        m_socket.open();
        m_socket.set_option(boost::asio::socket_base::receive_buffer_size(4 * 1024 * 1024));
        m_socket.bind(boost::asio::local::datagram_protocol::endpoint(m_path));
        m_socket.non_blocking(true);
    }

    ~DatagramReceiver() { std::filesystem::remove(m_path); }

    DatagramReceiver(const DatagramReceiver&) = delete;
    DatagramReceiver& operator=(const DatagramReceiver&) = delete;

    [[nodiscard]] const std::string& Path() const noexcept { return m_path; }

    // Every datagram queued right now. The child has exited, so nothing more will arrive.
    std::vector<std::string> ReceiveAll()
    {
        std::vector<std::string> datagrams;
        std::vector<char> buffer(64 * 1024);
        for (;;)
        {
            boost::system::error_code ec;
            const auto bytes = m_socket.receive(boost::asio::buffer(buffer), 0, ec);
            if (ec)
            {
                break;
            }
            datagrams.emplace_back(buffer.data(), bytes);
        }
        return datagrams;
    }

   private:
    std::string m_path;
    boost::asio::io_context m_ioContext;
    boost::asio::local::datagram_protocol::socket m_socket;
};

}  // namespace

// Death test suites run before all other tests, so the parent has not initialized the Writer or started any threads
// when it forks. Each child owns the process-wide Writer and exits normally, which runs the writer's exit flush.

// Records one meter with a buffer far larger than the line, then exits.
static void RecordOneCounterAndExit(const std::string& socketPath)
{
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0, 4096);
    Counter counter(MeterId("shutdown.counter"));
    counter.Increment();
    std::exit(0);
}

TEST(WriterShutdownDeathTest, SendsBufferedLinesAtExit)
{
    DatagramReceiver receiver("/tmp/spectator_test_writer_exit.sock");
    EXPECT_EXIT(RecordOneCounterAndExit(receiver.Path()), testing::ExitedWithCode(0), "");
    EXPECT_EQ(receiver.ReceiveAll(), std::vector<std::string>{"c:shutdown.counter:1.000000\n"});
}

// A thread records one meter and stays alive, blocked, while the main thread exits.
static void RecordFromLiveThreadAndExit(const std::string& socketPath)
{
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0,
                                       BufferConfig{.size = 1024, .flushInterval = std::chrono::hours(1)});
    static std::atomic<bool> written{false};
    std::thread(
        []
        {
            Counter counter(MeterId("live.thread"));
            counter.Increment();
            written = true;
            std::this_thread::sleep_for(std::chrono::hours(1));
        })
        .detach();
    while (!written.load())
    {
        std::this_thread::yield();
    }
    std::exit(0);
}

TEST(WriterShutdownDeathTest, SendsLinesOfThreadsStillRunningAtExit)
{
    DatagramReceiver receiver("/tmp/spectator_test_writer_live.sock");
    EXPECT_EXIT(RecordFromLiveThreadAndExit(receiver.Path()), testing::ExitedWithCode(0), "");
    EXPECT_EQ(receiver.ReceiveAll(), std::vector<std::string>{"c:live.thread:1.000000\n"});
}

// A thread keeps writing while the main thread exits and static destruction runs.
static void WriteThroughExit(const std::string& socketPath)
{
    WriterTestHelper::InitializeWriter(WriterType::Unix, socketPath, 0,
                                       BufferConfig{.size = 512, .flushInterval = std::chrono::milliseconds(1)});
    static std::atomic<int> writes{0};
    std::thread(
        []
        {
            Counter counter(MeterId("through.exit"));
            for (;;)
            {
                counter.Increment();
                ++writes;
            }
        })
        .detach();
    while (writes.load() < 1000)
    {
        std::this_thread::yield();
    }
    std::exit(0);
}

TEST(WriterShutdownDeathTest, ThreadWritingThroughExitDoesNotCrash)
{
    DatagramReceiver receiver("/tmp/spectator_test_writer_through.sock");
    EXPECT_EXIT(WriteThroughExit(receiver.Path()), testing::ExitedWithCode(0), "");
}

// Tests against a UDS server running in this process.
class WriterUdsTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        clear_uds_messages();
        uds_server_running = true;
        server_thread = std::thread([] { listen_for_uds_messages(kServerSocketPath); });
        ASSERT_TRUE(wait_for_uds_server());
    }

    void TearDown() override
    {
        uds_server_running = false;
        if (server_thread.joinable())
        {
            server_thread.join();
        }
    }

    std::thread server_thread;
};

TEST_F(WriterUdsTest, MultithreadedWrite)
{
    // Each 29-byte line fills the 30-byte buffer, so every line is its own datagram. The last line of each thread is
    // sent when the thread exits.
    WriterTestHelper::InitializeWriter(WriterType::Unix, kServerSocketPath, 0, 30);

    constexpr auto numThreads = 4;
    constexpr auto countersPerThread = 3;
    constexpr auto incrementsPerCounter = 5;
    constexpr auto expectedLines = numThreads * countersPerThread * incrementsPerCounter;

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++)
    {
        threads.emplace_back(
            [t]
            {
                for (int i = 0; i < countersPerThread; i++)
                {
                    Counter counter(MeterId(fmt::format("counter.thread{}.{}", t, i)));
                    for (int j = 0; j < incrementsPerCounter; j++)
                    {
                        counter.Increment();
                    }
                }
            });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    ASSERT_TRUE(WaitFor([] { return SplitLines(get_uds_messages()).size() >= expectedLines; }));
    const auto lines = SplitLines(get_uds_messages());
    EXPECT_EQ(lines.size(), static_cast<std::size_t>(expectedLines));
    const std::regex counter_regex(R"(c:counter\.thread\d+\.\d+:1.000000)");
    for (const auto& line : lines)
    {
        EXPECT_TRUE(std::regex_match(line, counter_regex)) << "Unexpected counter format: " << line;
    }
    for (const auto& datagram : get_uds_messages())
    {
        EXPECT_LE(datagram.size(), 30u);
    }
}

// The NDrive team found a deadlock when the buffer size matched the line size and several threads wrote at once.
// Each line here is exactly 21 bytes, the buffer size, so every datagram must be exactly one line.
TEST_F(WriterUdsTest, TenThreadsBufferSizeEqualToLineSize)
{
    WriterTestHelper::InitializeWriter(WriterType::Unix, kServerSocketPath, 0, 21);

    constexpr auto numThreads = 10;
    constexpr auto incrementsPerCounter = 10;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++)
    {
        threads.emplace_back(
            [t]
            {
                Counter counter(MeterId(fmt::format("ctr{:06d}", t)));
                for (int j = 0; j < incrementsPerCounter; j++)
                {
                    counter.Increment();
                }
            });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    ASSERT_TRUE(WaitFor([] { return get_uds_messages().size() >= numThreads * incrementsPerCounter; }));
    const auto datagrams = get_uds_messages();
    EXPECT_EQ(datagrams.size(), static_cast<std::size_t>(numThreads * incrementsPerCounter));
    for (const auto& datagram : datagrams)
    {
        EXPECT_EQ(datagram.size(), 21u) << datagram;
    }
}

TEST_F(WriterUdsTest, LinesOfALiveThreadArriveOnTheFlushInterval)
{
    WriterTestHelper::InitializeWriter(WriterType::Unix, kServerSocketPath, 0,
                                       BufferConfig{.size = 1024, .flushInterval = std::chrono::milliseconds(50)});

    // The thread waits for its line to arrive before it exits, so only the drain thread can have sent it.
    std::atomic<bool> arrivedWhileAlive{false};
    std::thread(
        [&arrivedWhileAlive]
        {
            Counter counter(MeterId("live.interval"));
            counter.Increment();
            arrivedWhileAlive = WaitFor([] { return !get_uds_messages().empty(); });
        })
        .join();

    EXPECT_TRUE(arrivedWhileAlive.load());
    EXPECT_EQ(get_uds_messages(), std::vector<std::string>{"c:live.interval:1.000000\n"});
}

// Tests against a UDP server running in this process.
class WriterUdpTest : public testing::Test
{
   protected:
    void SetUp() override
    {
        clear_udp_messages();
        udp_server_running = true;
        server_thread = std::thread([] { listen_for_udp_messages(kUdpServerPort); });
        ASSERT_TRUE(wait_for_udp_server());
    }

    void TearDown() override
    {
        udp_server_running = false;
        if (server_thread.joinable())
        {
            server_thread.join();
        }
    }

    std::thread server_thread;
};

TEST_F(WriterUdpTest, BufferedLinesFromManyThreadsArrive)
{
    WriterTestHelper::InitializeWriter(WriterType::UDP, "127.0.0.1", kUdpServerPort,
                                       BufferConfig{.size = 1024, .flushInterval = std::chrono::hours(1)});

    constexpr int numThreads = 4;
    constexpr int incrementsPerThread = 200;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++)
    {
        threads.emplace_back(
            [t]
            {
                Counter counter(MeterId(fmt::format("udp.thread{}", t)));
                for (int i = 0; i < incrementsPerThread; i++)
                {
                    counter.Increment();
                }
            });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    ASSERT_TRUE(WaitFor([] { return SplitLines(get_udp_messages()).size() >= numThreads * incrementsPerThread; }));
    EXPECT_EQ(SplitLines(get_udp_messages()).size(), static_cast<std::size_t>(numThreads * incrementsPerThread));
    for (const auto& datagram : get_udp_messages())
    {
        EXPECT_LE(datagram.size(), 1024u);
    }
}

TEST(WriterBufferingTest, CapacityIsClampedToTheTransportLimit)
{
    auto recording = std::make_shared<Recording>();
    WriterTestHelper::InitializeWithTransport(std::make_unique<RecordingWriter>(recording, 0, 100),
                                              BufferConfig{.size = 1000, .flushInterval = std::chrono::hours(1)});

    Counter counter(MeterId("clamp"));
    for (int i = 0; i < 50; ++i)
    {
        counter.Increment();
    }
    WriterTestHelper::Shutdown();

    EXPECT_EQ(recording->Lines().size(), 50u);
    EXPECT_GT(recording->Count(), 1u);
    for (const auto& payload : recording->Payloads())
    {
        EXPECT_LE(payload.size(), 100u);
    }
}

TEST(WriterBufferingTest, DropOnFullCountsDropsAndDrainsEarly)
{
    auto recording = std::make_shared<Recording>();
    WriterTestHelper::InitializeWithTransport(
        std::make_unique<RecordingWriter>(recording),
        BufferConfig{.size = 64, .flushInterval = std::chrono::hours(1), .dropOnFull = true});

    // Each line is 16 bytes, so four fill the buffer and the fifth is dropped.
    Counter counter(MeterId("drop"));
    for (int i = 0; i < 5; ++i)
    {
        counter.Increment();
    }
    EXPECT_EQ(WriterTestHelper::DroppedCount(), 1u);

    // The drop woke the drain thread, which sent the full buffer long before the one-hour interval.
    ASSERT_TRUE(WaitFor([&] { return recording->Count() == 1; }, 2s));
    const auto datagrams = recording->Datagrams();
    EXPECT_EQ(datagrams[0].payload, "c:drop:1.000000\nc:drop:1.000000\nc:drop:1.000000\nc:drop:1.000000\n");
    EXPECT_NE(datagrams[0].sender, std::this_thread::get_id()) << "in drop mode the writing thread never sends";
    WriterTestHelper::Shutdown();
}

TEST(WriterModesTest, MemoryWriterIsAlwaysUnbuffered)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory, "", 0, 4096);
    const auto* memoryWriter = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    ASSERT_NE(memoryWriter, nullptr);

    Counter counter(MeterId("memory.counter"));
    counter.Increment();
    EXPECT_EQ(memoryWriter->LastLine(), "c:memory.counter:1.000000\n");
    EXPECT_EQ(WriterTestHelper::RegisteredBufferCount(), 0u);
}

TEST(WriterModesTest, NoneWriterDiscardsEverything)
{
    WriterTestHelper::InitializeWriter(WriterType::Noop, "", 0, 4096);
    EXPECT_NE(dynamic_cast<NoopWriter*>(WriterTestHelper::GetImpl()), nullptr);

    Counter counter(MeterId("noop.counter"));
    counter.Increment();
    EXPECT_EQ(WriterTestHelper::RegisteredBufferCount(), 0u);
}

TEST(WriterModesTest, WritesAfterShutdownAreDropped)
{
    auto recording = std::make_shared<Recording>();
    WriterTestHelper::InitializeWithTransport(std::make_unique<RecordingWriter>(recording),
                                              BufferConfig{.size = 1024, .flushInterval = std::chrono::hours(1)});
    Counter(MeterId("before.shutdown")).Increment();

    WriterTestHelper::Shutdown();
    EXPECT_EQ(recording->Lines(), Lines{"c:before.shutdown:1.000000"});
    EXPECT_EQ(WriterTestHelper::GetImpl(), nullptr);

    Counter(MeterId("after.shutdown")).Increment();
    EXPECT_EQ(recording->Count(), 1u);
}

TEST(WriterThreadExitTest, ExitedThreadsAreSentAndForgotten)
{
    auto recording = std::make_shared<Recording>();
    WriterTestHelper::InitializeWithTransport(std::make_unique<RecordingWriter>(recording),
                                              BufferConfig{.size = 1024, .flushInterval = std::chrono::milliseconds(20)});

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([t] { Counter(MeterId(fmt::format("exit.thread{}", t))).Increment(); });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Each thread sent its line as it exited, and the drain thread then forgets its buffer.
    EXPECT_EQ(recording->Lines().size(), 4u);
    EXPECT_TRUE(WaitFor([] { return WriterTestHelper::RegisteredBufferCount() == 0; }));
    WriterTestHelper::Shutdown();
}

namespace {
struct WritesInDestructor
{
    ~WritesInDestructor() { Counter(MeterId("late.line")).Increment(); }
};
}  // namespace

TEST(WriterThreadExitTest, ThreadLocalDestructorRunningAfterTheWriterHandleStillWrites)
{
    auto recording = std::make_shared<Recording>();
    WriterTestHelper::InitializeWithTransport(std::make_unique<RecordingWriter>(recording),
                                              BufferConfig{.size = 1024, .flushInterval = std::chrono::hours(1)});

    std::thread(
        []
        {
            // Constructed before this thread's first write creates its writer handle, so it is destroyed after the
            // handle, and its destructor writes once the handle is gone.
            [[maybe_unused]] thread_local WritesInDestructor late;
            Counter(MeterId("early.line")).Increment();
        })
        .join();

    EXPECT_EQ(recording->Lines(), (Lines{"c:early.line:1.000000", "c:late.line:1.000000"}));
    WriterTestHelper::Shutdown();
}
