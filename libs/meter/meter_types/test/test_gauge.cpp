#include <gauge.h>
#include <writer_test_helper.h>

#include <gtest/gtest.h>

class GaugeTest : public testing::Test
{
   protected:
    MeterId tid = MeterId("gauge");
};

TEST_F(GaugeTest, Now)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    Gauge g(tid);
    EXPECT_TRUE(writer->IsEmpty());
    g.Set(1);
    EXPECT_EQ("g:gauge:1.000000\n", writer->LastLine());
}

TEST_F(GaugeTest, TTL)
{
    WriterTestHelper::InitializeWriter(WriterType::Memory);
    const auto* writer = dynamic_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    Gauge g(tid, 10);
    EXPECT_TRUE(writer->IsEmpty());
    g.Set(42);
    EXPECT_EQ("g,10:gauge:42.000000\n", writer->LastLine());
}