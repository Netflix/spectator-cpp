#include <libs/meter/meter_types/include/counter.h>
#include <libs/writer/writer_wrapper/writer_test_helper.h>

#include <gtest/gtest.h>

class CounterTest : public testing::Test
{
   protected:
    MeterId tid = MeterId("counter");
};

TEST_F(CounterTest, increment)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    Counter c(tid);
    EXPECT_TRUE(writer->IsEmpty());
    c.Increment();
    EXPECT_EQ("c:counter:1.000000\n", writer->LastLine());
    c.Increment(2);
    EXPECT_EQ("c:counter:2.000000\n", writer->LastLine());
}

TEST_F(CounterTest, incrementNegative)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    Counter c(tid);
    c.Increment(-1);
    EXPECT_TRUE(writer->IsEmpty());
}