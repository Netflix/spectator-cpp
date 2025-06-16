#include <libs/meter/meter_types/include/percentile_dist_summary.h>
#include <libs/writer/writer_wrapper/writer_test_helper.h>

#include <gtest/gtest.h>

class PercentileDistSummaryTest : public ::testing::Test
{
   protected:
    MeterId tid = MeterId("percentile_dist_summary");
};

TEST_F(PercentileDistSummaryTest, record)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    PercentileDistributionSummary pds(tid);
    EXPECT_TRUE(writer->IsEmpty());

    pds.Record(42);
    EXPECT_EQ("D:percentile_dist_summary:42\n", writer->LastLine());
}

TEST_F(PercentileDistSummaryTest, recordNegative)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    PercentileDistributionSummary pds(tid);
    EXPECT_TRUE(writer->IsEmpty());

    pds.Record(-42);
    EXPECT_TRUE(writer->IsEmpty());
}

TEST_F(PercentileDistSummaryTest, recordZero)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    PercentileDistributionSummary pds(tid);
    EXPECT_TRUE(writer->IsEmpty());

    pds.Record(0);
    EXPECT_EQ("D:percentile_dist_summary:0\n", writer->LastLine());
}