#include <dist_summary.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>

class DistSummaryTest : public testing::Test
{
   protected:
    MeterId tid = MeterId("dist_summary");
};

TEST_F(DistSummaryTest, record)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    DistributionSummary ds(tid);
    EXPECT_TRUE(writer->IsEmpty());

    ds.Record(42);
    EXPECT_EQ("d:dist_summary:42\n", writer->LastLine());
}

TEST_F(DistSummaryTest, recordNegative)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    DistributionSummary ds(tid);
    EXPECT_TRUE(writer->IsEmpty());

    ds.Record(-42);
    EXPECT_TRUE(writer->IsEmpty());
}

TEST_F(DistSummaryTest, recordZero)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    DistributionSummary ds(tid);
    EXPECT_TRUE(writer->IsEmpty());

    ds.Record(0);
    EXPECT_EQ("d:dist_summary:0\n", writer->LastLine());
}