#include <gtest/gtest.h>
#include <cstdint>

#include "../include/registry.h"
#include <libs/writer/writer_wrapper/include/writer_test_helper.h>

#include <libs/utils/include/util.h>

TEST(RegistryTest, Close)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto c = r.counter("counter");
    c.Increment();

    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());
    EXPECT_EQ("c:counter:1.000000", memoryWriter->LastLine());

    memoryWriter->Close();
    EXPECT_TRUE(memoryWriter->IsEmpty());
}

TEST(RegistryTest, AgeGauge)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto g1           = r.age_gauge("age_gauge");
    auto g2           = r.age_gauge("age_gauge", {{"my-tags", "bar"}});
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    EXPECT_TRUE(memoryWriter->IsEmpty());

    g1.Set(1);
    EXPECT_EQ("A:age_gauge:1", memoryWriter->LastLine());

    g2.Set(2);
    EXPECT_EQ("A:age_gauge,my-tags=bar:2", memoryWriter->LastLine());
}

TEST(RegistryTest, AgeGaugeWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto g = r.age_gauge_with_id(r.new_id("age_gauge", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(0);
    EXPECT_EQ("A:age_gauge,extra-tags=foo,my-tags=bar:0", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, Counter)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto c1           = r.counter("counter");
    auto c2           = r.counter("counter", {{"my-tags", "bar"}});
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    EXPECT_TRUE(memoryWriter->IsEmpty());

    c1.Increment();
    EXPECT_EQ("c:counter:1.000000", memoryWriter->LastLine());

    c2.Increment();
    EXPECT_EQ("c:counter,my-tags=bar:1.000000", memoryWriter->LastLine());

    c1.Increment(2);
    EXPECT_EQ("c:counter:2.000000", memoryWriter->LastLine());

    c2.Increment(2);
    EXPECT_EQ("c:counter,my-tags=bar:2.000000", memoryWriter->LastLine());

    r.counter("counter").Increment(3);
    EXPECT_EQ("c:counter:3.000000", memoryWriter->LastLine());
}

TEST(RegistryTest, CounterWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto c = r.counter_with_id(r.new_id("counter", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Increment();
    EXPECT_EQ("c:counter,extra-tags=foo,my-tags=bar:1.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());

    c.Increment(2);
    EXPECT_EQ("c:counter,extra-tags=foo,my-tags=bar:2.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());

    r.counter("counter", {{"my-tags", "bar"}}).Increment(3);
    EXPECT_EQ("c:counter,extra-tags=foo,my-tags=bar:3.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, DistributionSummary)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto d = r.distribution_summary("distribution_summary");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("d:distribution_summary:42", memoryWriter->LastLine());
}

TEST(RegistryTest, DistributionSummaryWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto d = r.distribution_summary_with_id(r.new_id("distribution_summary", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("d:distribution_summary,extra-tags=foo,my-tags=bar:42", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, Gauge)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto g = r.gauge("gauge");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("g:gauge:42.000000", memoryWriter->LastLine());
}

TEST(RegistryTest, GaugeWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto g = r.gauge_with_id(r.new_id("gauge", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("g:gauge,extra-tags=foo,my-tags=bar:42.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, GaugeWithIdWithTtlSeconds)
{
    // WriterConfig writerConfig(WriterTypes::Memory);
    // Config config(writerConfig, {{"extra-tags", "foo"}});
    // auto r = Registry(config);

    // auto g = r.gauge_with_id(r.new_id("gauge", {{"my-tags", "bar"}}), 120);
    // EXPECT_TRUE(memoryWriter->IsEmpty());

    // g->Set(42);
    // EXPECT_EQ("g,120:gauge,extra-tags=foo,my-tags=bar:42", memoryWriter->LastLine());
}

// TEST_F(RegistryTest, GaugeWithTtlSeconds) {
//     WriterConfig writerConfig(WriterTypes::Memory);
//     Config config(writerConfig);
//     auto r = Registry(config);

//     auto g = r.gauge("gauge", 120);
//     EXPECT_TRUE(memoryWriter->IsEmpty());

//     g.Set(42);
//     EXPECT_EQ("g,120:gauge:42", memoryWriter->LastLine());
// }

TEST(RegistryTest, MaxGauge)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto g = r.max_gauge("max_gauge");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("m:max_gauge:42.000000", memoryWriter->LastLine());
}

TEST(RegistryTest, MaxGaugeWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto g = r.max_gauge_with_id(r.new_id("max_gauge", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("m:max_gauge,extra-tags=foo,my-tags=bar:42.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, MonotonicCounter)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto c = r.monotonic_counter("monotonic_counter");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("C:monotonic_counter:42.000000", memoryWriter->LastLine());
}

TEST(RegistryTest, MonotonicCounterWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto c = r.monotonic_counter_with_id(r.new_id("monotonic_counter", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("C:monotonic_counter,extra-tags=foo,my-tags=bar:42.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, MonotonicCounterUint)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto c = r.monotonic_counter_uint("monotonic_counter_uint");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("U:monotonic_counter_uint:42", memoryWriter->LastLine());
}

TEST(RegistryTest, MonotonicCounterUintWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto c = r.monotonic_counter_uint_with_id(r.new_id("monotonic_counter_uint", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("U:monotonic_counter_uint,extra-tags=foo,my-tags=bar:42", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, NewId)
{
    Config config1(WriterConfig(WriterTypes::Memory));
    auto r1  = Registry(config1);
    auto id1 = r1.new_id("id");
    EXPECT_EQ("MeterId(name=id, tags={})", id1.to_string());

    Config config2(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r2  = Registry(config2);
    auto id2 = r2.new_id("id");
    EXPECT_EQ("MeterId(name=id, tags={'extra-tags': 'foo'})", id2.to_string());
}

TEST(RegistryTest, PctDistributionSummary)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto d = r.pct_distribution_summary("pct_distribution_summary");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("D:pct_distribution_summary:42", memoryWriter->LastLine());
}

TEST(RegistryTest, PctDistributionSummaryWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto d = r.pct_distribution_summary_with_id(r.new_id("pct_distribution_summary", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("D:pct_distribution_summary,extra-tags=foo,my-tags=bar:42", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, PctTimer)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto t = r.pct_timer("pct_timer");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("T:pct_timer:42.000000", memoryWriter->LastLine());
}

TEST(RegistryTest, PctTimerWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto t = r.pct_timer_with_id(r.new_id("pct_timer", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("T:pct_timer,extra-tags=foo,my-tags=bar:42.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, Timer)
{
    Config config(WriterConfig(WriterTypes::Memory));
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto t = r.timer("timer");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("t:timer:42.000000", memoryWriter->LastLine());
}

TEST(RegistryTest, TimerWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r            = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter *>(WriterTestHelper::GetImpl());

    auto t = r.timer_with_id(r.new_id("timer", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("t:timer,extra-tags=foo,my-tags=bar:42.000000", ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}