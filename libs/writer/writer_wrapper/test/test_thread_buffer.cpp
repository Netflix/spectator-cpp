#include <thread_buffer.h>

#include <recording_writer.h>

#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace spectator;
using spectator::detail::Sink;
using spectator::detail::ThreadBuffer;
using spectator::test::Recording;
using spectator::test::RecordingWriter;
using Result = ThreadBuffer::AppendResult;
using Payloads = std::vector<std::string>;

class ThreadBufferTest : public testing::Test
{
   protected:
    std::shared_ptr<Recording> recording = std::make_shared<Recording>();
    std::shared_ptr<Sink> sink = std::make_shared<Sink>(std::make_unique<RecordingWriter>(recording));
};

TEST_F(ThreadBufferTest, HoldsLinesUntilDrained)
{
    ThreadBuffer buffer(sink, 64, false);
    EXPECT_EQ(buffer.Append("c:a:1"), Result::Appended);
    EXPECT_EQ(buffer.Append("c:b:2"), Result::Appended);
    EXPECT_EQ(recording->Count(), 0u);

    std::string scratch;
    buffer.SendPending(scratch);
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:a:1\nc:b:2\n"}));

    // Nothing is pending, so nothing more is sent.
    buffer.SendPending(scratch);
    EXPECT_EQ(recording->Count(), 1u);
}

TEST_F(ThreadBufferTest, ExactFitStaysBufferedAndOneMoreByteFlushes)
{
    ThreadBuffer buffer(sink, 8, false);
    EXPECT_EQ(buffer.Append("abc"), Result::Appended);
    EXPECT_EQ(buffer.Append("def"), Result::Appended);
    EXPECT_EQ(recording->Count(), 0u) << "8 bytes fit a capacity of 8 exactly";

    // The next line doesn't fit, so this thread sends the full buffer itself and keeps the new line.
    EXPECT_EQ(buffer.Append("g"), Result::Appended);
    const auto datagrams = recording->Datagrams();
    ASSERT_EQ(datagrams.size(), 1u);
    EXPECT_EQ(datagrams[0].payload, "abc\ndef\n");
    EXPECT_EQ(datagrams[0].sender, std::this_thread::get_id());

    buffer.FlushAndClose();
    EXPECT_EQ(recording->Payloads(), (Payloads{"abc\ndef\n", "g\n"}));
}

TEST_F(ThreadBufferTest, DropOnFullDropsTheLineAndKeepsTheBuffer)
{
    ThreadBuffer buffer(sink, 8, true);
    EXPECT_EQ(buffer.Append("abc"), Result::Appended);
    EXPECT_EQ(buffer.Append("def"), Result::Appended);
    EXPECT_EQ(buffer.Append("g"), Result::Dropped);
    EXPECT_EQ(recording->Count(), 0u) << "a dropped line never makes the writing thread send";

    std::string scratch;
    buffer.SendPending(scratch);
    EXPECT_EQ(recording->Payloads(), (Payloads{"abc\ndef\n"}));
}

TEST_F(ThreadBufferTest, OversizedLineIsSentAloneAfterPendingLines)
{
    for (const bool dropOnFull : {false, true})
    {
        SCOPED_TRACE(dropOnFull ? "drop on full" : "flush on full");
        auto testRecording = std::make_shared<Recording>();
        ThreadBuffer buffer(std::make_shared<Sink>(std::make_unique<RecordingWriter>(testRecording)), 8, dropOnFull);

        EXPECT_EQ(buffer.Append("ab"), Result::Appended);
        const std::string oversized(20, 'x');
        EXPECT_EQ(buffer.Append(oversized), Result::Appended);
        EXPECT_EQ(testRecording->Payloads(), (Payloads{"ab\n", oversized + "\n"}));

        // The buffer is empty again and keeps buffering.
        EXPECT_EQ(buffer.Append("c"), Result::Appended);
        EXPECT_EQ(testRecording->Count(), 2u);
    }
}

TEST_F(ThreadBufferTest, CloseSendsPendingLinesAndRejectsLaterAppends)
{
    ThreadBuffer buffer(sink, 64, false);
    EXPECT_EQ(buffer.Append("c:a:1"), Result::Appended);

    buffer.FlushAndClose();
    EXPECT_TRUE(buffer.IsClosed());
    EXPECT_EQ(recording->Payloads(), (Payloads{"c:a:1\n"}));
    EXPECT_EQ(buffer.Append("c:b:1"), Result::Closed);

    // Closing again and draining a closed buffer send nothing.
    buffer.FlushAndClose();
    std::string scratch;
    buffer.SendPending(scratch);
    EXPECT_EQ(recording->Count(), 1u);
}

TEST_F(ThreadBufferTest, CloseWithNothingPendingSendsNothing)
{
    ThreadBuffer buffer(sink, 64, false);
    buffer.FlushAndClose();
    EXPECT_TRUE(buffer.IsClosed());
    EXPECT_EQ(recording->Count(), 0u);
}

TEST_F(ThreadBufferTest, LinesStayInOrderWhileTheDrainRacesTheWriter)
{
    // A small capacity makes the writing thread flush often, while another thread drains in a tight loop. Every line
    // must still arrive exactly once and in the order it was written.
    constexpr std::size_t kCapacity = 32;
    constexpr int kLines = 20000;
    ThreadBuffer buffer(sink, kCapacity, false);

    std::atomic<bool> stop{false};
    std::thread drainer(
        [&]
        {
            std::string scratch;
            while (!stop.load())
            {
                buffer.SendPending(scratch);
            }
        });
    for (int i = 0; i < kLines; ++i)
    {
        ASSERT_EQ(buffer.Append(std::to_string(i)), Result::Appended);
    }
    stop = true;
    drainer.join();
    buffer.FlushAndClose();

    const auto lines = recording->Lines();
    ASSERT_EQ(lines.size(), static_cast<std::size_t>(kLines));
    for (int i = 0; i < kLines; ++i)
    {
        ASSERT_EQ(lines[i], std::to_string(i)) << "line " << i << " arrived out of order";
    }
    for (const auto& payload : recording->Payloads())
    {
        EXPECT_LE(payload.size(), kCapacity);
    }
}
