#include <libs/meter/meter_types/include/max_gauge.h>
#include <libs/writer/writer_wrapper/writer_test_helper.h>

#include <gtest/gtest.h>

class MaxGaugeTest : public ::testing::Test
{
   protected:
    MeterId tid = MeterId("max_gauge");
};

TEST_F(MaxGaugeTest, Set)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    MaxGauge g(tid);
    EXPECT_TRUE(writer->IsEmpty());
    g.Set(0);
    EXPECT_EQ("m:max_gauge:0.000000", writer->LastLine());
}