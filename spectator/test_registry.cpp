#include <gtest/gtest.h>
#include <cstdint>

#include <registry.h>
#include <writer_test_helper.h>

#include <util.h>

TEST(RegistryTest, Close)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto c = r.CreateCounter("counter");
    c.Increment();

    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    EXPECT_EQ("c:counter:1.000000\n", memoryWriter->LastLine());

    memoryWriter->Close();
    EXPECT_TRUE(memoryWriter->IsEmpty());
}

TEST(RegistryTest, AgeGauge)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto g1 = r.CreateAgeGauge("age_gauge");
    auto g2 = r.CreateAgeGauge("age_gauge", {{"my-tags", "bar"}});
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    EXPECT_TRUE(memoryWriter->IsEmpty());

    g1.Set(1);
    EXPECT_EQ("A:age_gauge:1.000000\n", memoryWriter->LastLine());

    g2.Set(2);
    EXPECT_EQ("A:age_gauge,my-tags=bar:2.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, AgeGaugeWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto g = r.CreateAgeGauge(r.CreateNewId("age_gauge", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(0);
    EXPECT_EQ("A:age_gauge,extra-tags=foo,my-tags=bar:0.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, Counter)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto c1 = r.CreateCounter("counter");
    auto c2 = r.CreateCounter("counter", {{"my-tags", "bar"}});
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    EXPECT_TRUE(memoryWriter->IsEmpty());

    c1.Increment();
    EXPECT_EQ("c:counter:1.000000\n", memoryWriter->LastLine());

    c2.Increment();
    EXPECT_EQ("c:counter,my-tags=bar:1.000000\n", memoryWriter->LastLine());

    c1.Increment(2);
    EXPECT_EQ("c:counter:2.000000\n", memoryWriter->LastLine());

    c2.Increment(2);
    EXPECT_EQ("c:counter,my-tags=bar:2.000000\n", memoryWriter->LastLine());

    r.CreateCounter("counter").Increment(3);
    EXPECT_EQ("c:counter:3.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, CounterWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto c = r.CreateCounter(r.CreateNewId("counter", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Increment();
    EXPECT_EQ("c:counter,extra-tags=foo,my-tags=bar:1.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());

    c.Increment(2);
    EXPECT_EQ("c:counter,extra-tags=foo,my-tags=bar:2.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());

    r.CreateCounter("counter", {{"my-tags", "bar"}}).Increment(3);
    EXPECT_EQ("c:counter,extra-tags=foo,my-tags=bar:3.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, DistributionSummary)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto d = r.CreateDistributionSummary("distribution_summary");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("d:distribution_summary:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, DistributionSummaryWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto d = r.CreateDistributionSummary(r.CreateNewId("distribution_summary", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("d:distribution_summary,extra-tags=foo,my-tags=bar:42.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, Gauge)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto g = r.CreateGauge("gauge");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("g:gauge:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, GaugeWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto g = r.CreateGauge(r.CreateNewId("gauge", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("g:gauge,extra-tags=foo,my-tags=bar:42.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, GaugeWithIdWithTtlSeconds)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());
    
    auto g = r.CreateGauge(r.CreateNewId("gauge", {{"my-tags", "bar"}}), 120);
    EXPECT_TRUE(memoryWriter->IsEmpty());
    g.Set(42);
    EXPECT_EQ("g,120:gauge,extra-tags=foo,my-tags=bar:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, GaugeWithTtlSeconds) {
    Config config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto g = r.CreateGauge("gauge", {}, 120);
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("g,120:gauge:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, MaxGauge)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto g = r.CreateMaxGauge("max_gauge");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("m:max_gauge:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, MaxGaugeWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto g = r.CreateMaxGauge(r.CreateNewId("max_gauge", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    g.Set(42);
    EXPECT_EQ("m:max_gauge,extra-tags=foo,my-tags=bar:42.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, MonotonicCounter)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto c = r.CreateMonotonicCounter("monotonic_counter");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("C:monotonic_counter:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, MonotonicCounterWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto c = r.CreateMonotonicCounter(r.CreateNewId("monotonic_counter", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("C:monotonic_counter,extra-tags=foo,my-tags=bar:42.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, MonotonicCounterUint)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto c = r.CreateMonotonicCounterUint("monotonic_counter_uint");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("U:monotonic_counter_uint:42\n", memoryWriter->LastLine());
}

TEST(RegistryTest, MonotonicCounterUintWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto c = r.CreateMonotonicCounterUint(r.CreateNewId("monotonic_counter_uint", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    c.Set(42);
    EXPECT_EQ("U:monotonic_counter_uint,extra-tags=foo,my-tags=bar:42\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, NewId)
{
    auto config1 = Config(WriterConfig(WriterTypes::Memory));
    auto r1 = Registry(config1);
    auto id1 = r1.CreateNewId("id");
    EXPECT_EQ("MeterId(name=id, tags={})", id1.to_string());

    Config config2(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r2 = Registry(config2);
    auto id2 = r2.CreateNewId("id");
    EXPECT_EQ("MeterId(name=id, tags={'extra-tags': 'foo'})", id2.to_string());
}

TEST(RegistryTest, PctDistributionSummary)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto d = r.CreatePercentDistributionSummary("pct_distribution_summary");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("D:pct_distribution_summary:42\n", memoryWriter->LastLine());
}

TEST(RegistryTest, PctDistributionSummaryWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto d = r.CreatePercentDistributionSummary(r.CreateNewId("pct_distribution_summary", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    d.Record(42);
    EXPECT_EQ("D:pct_distribution_summary,extra-tags=foo,my-tags=bar:42\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, PctTimer)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto t = r.CreatePercentTimer("pct_timer");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("T:pct_timer:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, PctTimerWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto t = r.CreatePercentTimer(r.CreateNewId("pct_timer", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("T:pct_timer,extra-tags=foo,my-tags=bar:42.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}

TEST(RegistryTest, Timer)
{
    auto config = Config(WriterConfig(WriterTypes::Memory));
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto t = r.CreateTimer("timer");
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("t:timer:42.000000\n", memoryWriter->LastLine());
}

TEST(RegistryTest, TimerWithId)
{
    Config config(WriterConfig(WriterTypes::Memory), {{"extra-tags", "foo"}});
    auto r = Registry(config);
    auto memoryWriter = static_cast<MemoryWriter*>(WriterTestHelper::GetImpl());

    auto t = r.CreateTimer(r.CreateNewId("timer", {{"my-tags", "bar"}}));
    EXPECT_TRUE(memoryWriter->IsEmpty());

    t.Record(42);
    EXPECT_EQ("t:timer,extra-tags=foo,my-tags=bar:42.000000\n",
              ParseProtocolLine(memoryWriter->LastLine()).value().to_string());
}