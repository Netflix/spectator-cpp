#include <libs/meter/meter_types/include/monotonic_counter_uint.h>
#include <libs/writer/writer_wrapper/writer_test_helper.h>

#include <gtest/gtest.h>

class MonoCounterTest : public ::testing::Test
{
   protected:
    MeterId tid = MeterId("monotonic_counter_uint");
};

TEST_F(MonoCounterTest, set)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    MonotonicCounterUint mc(tid);
    EXPECT_TRUE(writer->IsEmpty());

    mc.Set(1);
    EXPECT_EQ("U:monotonic_counter_uint:1\n", writer->LastLine());
}

TEST_F(MonoCounterTest, setNegative)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    MonotonicCounterUint mc(tid);
    EXPECT_TRUE(writer->IsEmpty());

    mc.Set(-1);
    EXPECT_EQ("U:monotonic_counter_uint:18446744073709551615\n", writer->LastLine());
}