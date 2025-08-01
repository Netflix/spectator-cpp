#include <age_gauge.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>

class AgeGaugeTest : public testing::Test
{
   protected:
    MeterId tid = MeterId("age_gauge");
};

TEST_F(AgeGaugeTest, Now)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    AgeGauge g(tid);
    EXPECT_TRUE(writer->IsEmpty());
    g.Now();
    EXPECT_EQ("A:age_gauge:0\n", writer->LastLine());
}

TEST_F(AgeGaugeTest, Set)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    AgeGauge g(tid);
    EXPECT_TRUE(writer->IsEmpty());
    g.Set(10);
    EXPECT_EQ("A:age_gauge:10.000000\n", writer->LastLine());
}