#include <memory_writer.h>
#include <gtest/gtest.h>

TEST(MemoryWriterTest, IsEmpty)
{
    const auto writer = MemoryWriter();
    EXPECT_TRUE(writer.IsEmpty());
}

TEST(MemoryWriterTest, Write)
{
    auto writer = MemoryWriter();
    writer.Write("Test message");
    EXPECT_FALSE(writer.IsEmpty());
    EXPECT_EQ(writer.LastLine(), "Test message");
}

TEST(MemoryWriterTest, Clear)
{
    auto writer = MemoryWriter();
    writer.Write("Test message");
    EXPECT_FALSE(writer.IsEmpty());

    writer.Clear();
    EXPECT_TRUE(writer.IsEmpty());
}

TEST(MemoryWriterTest, GetMessages)
{
    auto writer = MemoryWriter();
    writer.Write("First message");
    writer.Write("Second message");

    const auto& messages = writer.GetMessages();
    EXPECT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0], "First message");
    EXPECT_EQ(messages[1], "Second message");
}

TEST(MemoryWriterTest, LastLine)
{
    auto writer = MemoryWriter();
    writer.Write("First message");
    writer.Write("Second message");

    EXPECT_EQ(writer.LastLine(), "Second message");

    writer.Clear();
    EXPECT_EQ(writer.LastLine(), "");
}