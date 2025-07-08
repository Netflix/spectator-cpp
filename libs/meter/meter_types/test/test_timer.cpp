#include <timer.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>

class TimerTest : public testing::Test
{
   protected:
    MeterId tid = MeterId("timer");
};

TEST_F(TimerTest, record)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    Timer t(tid);
    EXPECT_TRUE(writer->IsEmpty());

    t.Record(42);
    EXPECT_EQ("t:timer:42.000000\n", writer->LastLine());
}

TEST_F(TimerTest, recordNegative)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    Timer t(tid);
    EXPECT_TRUE(writer->IsEmpty());

    t.Record(-42);
    EXPECT_TRUE(writer->IsEmpty());
}

TEST_F(TimerTest, recordZero)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    Timer t(tid);
    EXPECT_TRUE(writer->IsEmpty());

    t.Record(0);
    EXPECT_EQ("t:timer:0.000000\n", writer->LastLine());
}