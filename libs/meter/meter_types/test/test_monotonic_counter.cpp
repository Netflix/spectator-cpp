#include <monotonic_counter.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>

class MonotonicCounterTest : public testing::Test
{
   protected:
    MeterId tid = MeterId("monotonic_counter");
};

TEST_F(MonotonicCounterTest, SetValue)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    MonotonicCounter mc(tid);
    EXPECT_TRUE(writer->IsEmpty());
    mc.Set(1);
    EXPECT_EQ("C:monotonic_counter:1.000000\n", writer->LastLine());
}

TEST_F(MonotonicCounterTest, SetNegativeValue)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    MonotonicCounter mc(tid);
    EXPECT_TRUE(writer->IsEmpty());
    mc.Set(-1);
    EXPECT_EQ("C:monotonic_counter:-1.000000\n", writer->LastLine());
}