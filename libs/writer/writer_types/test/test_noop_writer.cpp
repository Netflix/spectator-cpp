#include <noop_writer.h>

#include <gtest/gtest.h>
#include <limits>
#include <string>

using namespace spectator;

TEST(NoopWriterTest, AcceptsWritesAndClose)
{
    NoopWriter writer;
    writer.Write("c:counter:1.000000\n");
    writer.Write(std::string(100000, 'x'));
    writer.Close();
    writer.Write("after close");
}

TEST(NoopWriterTest, HasNoPayloadLimit)
{
    const NoopWriter writer;
    EXPECT_EQ(writer.MaxPayloadSize(), std::numeric_limits<std::size_t>::max());
}
