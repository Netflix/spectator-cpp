#include <counter.h>
#include <dist_summary.h>
#include <gauge.h>
#include <meter_id.h>
#include <registry.h>
#include <timer.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace spectator;

// ----------------------------------------------------------------------------
// Benchmark harness — mirrors Go's testing.B: runs for ~1s, reports ns/op
// ----------------------------------------------------------------------------

struct BenchResult
{
    std::string name;
    long long   iterations;
    double      ns_per_op;
};

template <typename Fn>
BenchResult RunBench(const std::string& name, Fn fn)
{
    for (int i = 0; i < 100; ++i) fn();

    const auto budget = std::chrono::seconds(1);
    long long  iters  = 0;
    const auto start  = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < budget)
    {
        for (int i = 0; i < 1000; ++i) fn();
        iters += 1000;
    }

    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    return {name, iters, (elapsed * 1e9) / static_cast<double>(iters)};
}

void PrintResult(const BenchResult& r)
{
    std::cout << std::left  << std::setw(52) << r.name
              << std::right << std::setw(12) << r.iterations
              << "  " << std::fixed << std::setprecision(1)
              << std::setw(10) << r.ns_per_op << " ns/op\n";
}

// ----------------------------------------------------------------------------
// Pre-generated varying tag values (avoids measuring std::to_string overhead)
// ----------------------------------------------------------------------------

static std::string varyingValues[1024];
static int         varyingIdx = 0;

static void InitVaryingValues()
{
    for (int i = 0; i < 1024; ++i)
        varyingValues[i] = "value-" + std::to_string(i);
}

static std::unordered_map<std::string, std::string> MakeTags(int n)
{
    std::unordered_map<std::string, std::string> tags;
    tags.reserve(n);
    for (int i = 0; i < n; ++i)
        tags["key" + std::to_string(i)] = "val" + std::to_string(i);
    return tags;
}

// ----------------------------------------------------------------------------
// Id creation benchmarks
// ----------------------------------------------------------------------------

BenchResult BenchmarkNewId()
{
    const std::unordered_map<std::string, std::string> tags = {
        {"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}};
    return RunBench("BenchmarkNewId", [&] {
        MeterId("test.metric.name", tags);
    });
}

BenchResult BenchmarkNewId_ManyTags()
{
    auto tags = MakeTags(10);
    return RunBench("BenchmarkNewId_ManyTags", [&] {
        MeterId("test.metric.name", tags);
    });
}

BenchResult BenchmarkWithTag()
{
    MeterId baseId("test.metric.name", {{"app", "myapp"}, {"region", "us-east-1"}});
    return RunBench("BenchmarkWithTag", [&] {
        baseId.WithTag("instance", varyingValues[varyingIdx++ & 1023]);
    });
}

BenchResult BenchmarkWithTags()
{
    MeterId baseId("test.metric.name", {{"app", "myapp"}});
    const std::unordered_map<std::string, std::string> extra = {
        {"region", "us-east-1"}, {"env", "prod"}, {"cluster", "main"}};
    return RunBench("BenchmarkWithTags", [&] {
        baseId.WithTags(extra);
    });
}

std::vector<BenchResult> BenchmarkNewId_TagScaling()
{
    std::vector<BenchResult> results;
    for (int n : {0, 1, 3, 5, 10})
    {
        auto tags = MakeTags(n);
        results.push_back(RunBench("BenchmarkNewId_TagScaling/tags-" + std::to_string(n), [&] {
            MeterId("test.metric.name", tags);
        }));
    }
    return results;
}

// ----------------------------------------------------------------------------
// Counter benchmarks
// ----------------------------------------------------------------------------

BenchResult BenchmarkNewCounter()
{
    const std::unordered_map<std::string, std::string> tags = {
        {"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}};
    return RunBench("BenchmarkNewCounter", [&] {
        Counter(MeterId("test.counter", tags));
    });
}

BenchResult BenchmarkCounter_Increment()
{
    Counter c(MeterId("test.counter",
        {{"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}}));
    return RunBench("BenchmarkCounter_Increment", [&] {
        c.Increment();
    });
}

BenchResult BenchmarkCounter_Add()
{
    Counter c(MeterId("test.counter",
        {{"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}}));
    return RunBench("BenchmarkCounter_Add", [&] {
        c.Increment(42);
    });
}

BenchResult BenchmarkCounter_VaryingTags()
{
    return RunBench("BenchmarkCounter_VaryingTags", [] {
        Counter(MeterId("test.counter",
            {{"app", "myapp"}, {"instance", varyingValues[varyingIdx++ & 1023]}})).Increment();
    });
}

// ----------------------------------------------------------------------------
// Gauge benchmarks
// ----------------------------------------------------------------------------

BenchResult BenchmarkNewGauge()
{
    const std::unordered_map<std::string, std::string> tags = {
        {"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}};
    return RunBench("BenchmarkNewGauge", [&] {
        Gauge(MeterId("test.gauge", tags));
    });
}

BenchResult BenchmarkGauge_Set()
{
    Gauge g(MeterId("test.gauge",
        {{"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}}));
    return RunBench("BenchmarkGauge_Set", [&] {
        g.Set(3.14159);
    });
}

BenchResult BenchmarkGauge_VaryingTags()
{
    return RunBench("BenchmarkGauge_VaryingTags", [] {
        Gauge(MeterId("test.gauge",
            {{"app", "myapp"}, {"instance", varyingValues[varyingIdx++ & 1023]}})).Set(3.14159);
    });
}

// ----------------------------------------------------------------------------
// DistributionSummary benchmarks
// ----------------------------------------------------------------------------

BenchResult BenchmarkNewDistributionSummary()
{
    const std::unordered_map<std::string, std::string> tags = {
        {"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}};
    return RunBench("BenchmarkNewDistributionSummary", [&] {
        DistributionSummary(MeterId("test.distsummary", tags));
    });
}

BenchResult BenchmarkDistributionSummary_Record()
{
    DistributionSummary d(MeterId("test.distsummary",
        {{"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}}));
    return RunBench("BenchmarkDistributionSummary_Record", [&] {
        d.Record(42);
    });
}

BenchResult BenchmarkDistributionSummary_VaryingTags()
{
    return RunBench("BenchmarkDistributionSummary_VaryingTags", [] {
        DistributionSummary(MeterId("test.distsummary",
            {{"app", "myapp"}, {"instance", varyingValues[varyingIdx++ & 1023]}})).Record(42);
    });
}

// ----------------------------------------------------------------------------
// Timer benchmarks
// ----------------------------------------------------------------------------

BenchResult BenchmarkNewTimer()
{
    const std::unordered_map<std::string, std::string> tags = {
        {"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}};
    return RunBench("BenchmarkNewTimer", [&] {
        Timer(MeterId("test.timer", tags));
    });
}

BenchResult BenchmarkTimer_Record()
{
    Timer t(MeterId("test.timer",
        {{"app", "myapp"}, {"region", "us-east-1"}, {"env", "prod"}}));
    return RunBench("BenchmarkTimer_Record", [&] {
        t.Record(42.0);
    });
}

BenchResult BenchmarkTimer_VaryingTags()
{
    return RunBench("BenchmarkTimer_VaryingTags", [] {
        Timer(MeterId("test.timer",
            {{"app", "myapp"}, {"instance", varyingValues[varyingIdx++ & 1023]}})).Record(42.0);
    });
}

// ----------------------------------------------------------------------------

int main()
{
    InitVaryingValues();

    // Initialize the Writer singleton once before any benchmarks.
    // Meters need the singleton initialized but don't use the registry after that.
    Registry initReg{Config{WriterConfig{WriterTypes::Noop}}};

    std::vector<BenchResult> results;

    results.push_back(BenchmarkNewId());
    results.push_back(BenchmarkNewId_ManyTags());
    results.push_back(BenchmarkWithTag());
    results.push_back(BenchmarkWithTags());
    for (auto& r : BenchmarkNewId_TagScaling()) results.push_back(r);

    results.push_back(BenchmarkNewCounter());
    results.push_back(BenchmarkCounter_Increment());
    results.push_back(BenchmarkCounter_Add());
    results.push_back(BenchmarkCounter_VaryingTags());

    results.push_back(BenchmarkNewGauge());
    results.push_back(BenchmarkGauge_Set());
    results.push_back(BenchmarkGauge_VaryingTags());

    results.push_back(BenchmarkNewDistributionSummary());
    results.push_back(BenchmarkDistributionSummary_Record());
    results.push_back(BenchmarkDistributionSummary_VaryingTags());

    results.push_back(BenchmarkNewTimer());
    results.push_back(BenchmarkTimer_Record());
    results.push_back(BenchmarkTimer_VaryingTags());

    std::cout << "\n";
    std::cout << std::left  << std::setw(52) << "Benchmark"
              << std::right << std::setw(12) << "Iterations"
              << "     ns/op\n";
    std::cout << std::string(78, '-') << "\n";
    for (const auto& r : results) PrintResult(r);
    std::cout << "\n";

    return 0;
}
