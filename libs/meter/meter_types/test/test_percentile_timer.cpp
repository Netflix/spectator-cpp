#include <libs/meter/meter_types/include/percentile_timer.h>
#include <libs/writer/writer_wrapper/include/writer_test_helper.h>

#include <gtest/gtest.h>

class PercentileTimerTest : public ::testing::Test
{
  protected:
    MeterId tid = MeterId("percentile_timer");
};

TEST_F(PercentileTimerTest, record)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto *writer = dynamic_cast<MemoryWriter *>(WriterTestHelper::GetImpl());
    PercentileTimer pt(tid);
    EXPECT_TRUE(writer->IsEmpty());

    pt.Record(42);
    EXPECT_EQ("T:percentile_timer:42.000000", writer->LastLine());
}

TEST_F(PercentileTimerTest, recordNegative)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto *writer = dynamic_cast<MemoryWriter *>(WriterTestHelper::GetImpl());
    PercentileTimer pt(tid);
    EXPECT_TRUE(writer->IsEmpty());

    pt.Record(-42);
    EXPECT_TRUE(writer->IsEmpty());
}

TEST_F(PercentileTimerTest, recordZero)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto *writer = dynamic_cast<MemoryWriter *>(WriterTestHelper::GetImpl());
    PercentileTimer pt(tid);
    EXPECT_TRUE(writer->IsEmpty());

    pt.Record(0);
    EXPECT_EQ("T:percentile_timer:0.000000", writer->LastLine());
}