#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <registry.h>
#include <config.h>
#include <writer.h>
#include <writer_config.h>

#include <string>
#include <unordered_map>

using namespace emscripten;
using Tags = std::unordered_map<std::string, std::string>;

// Convert a JS object {key: "value"} to a C++ tags map using emscripten::val.
static Tags TagsFromVal(const val& obj)
{
    Tags tags;
    if (obj.isNull() || obj.isUndefined()) return tags;

    val keys = val::global("Object").call<val>("keys", obj);
    const unsigned int len = keys["length"].as<unsigned int>();
    tags.reserve(len);

    for (unsigned int i = 0; i < len; ++i)
    {
        std::string k = keys[i].as<std::string>();
        std::string v = obj[k].as<std::string>();
        tags.emplace(std::move(k), std::move(v));
    }
    return tags;
}

// ---------------------------------------------------------------------------
// WasmRegistry — always uses the memory writer.
// Browsers cannot open raw UDP/Unix sockets, so metrics are accumulated
// in memory and returned via flush() for the JS layer to relay via fetch/WS.
// ---------------------------------------------------------------------------

struct WasmRegistry
{
    spectator::Registry reg;

    WasmRegistry()
        : reg(spectator::Config(spectator::WriterConfig("memory")))
    {}

    void counterIncrement(const std::string& name, const val& tags)
    {
        reg.CreateCounter(name, TagsFromVal(tags)).Increment();
    }

    void counterAdd(const std::string& name, int64_t delta, const val& tags)
    {
        reg.CreateCounter(name, TagsFromVal(tags)).Increment(delta);
    }

    void gaugeSet(const std::string& name, double value, const val& tags)
    {
        reg.CreateGauge(name, TagsFromVal(tags)).Set(value);
    }

    void gaugeSetTtl(const std::string& name, double value, int ttl, const val& tags)
    {
        reg.CreateGauge(name, TagsFromVal(tags), ttl).Set(value);
    }

    void maxGaugeSet(const std::string& name, double value, const val& tags)
    {
        reg.CreateMaxGauge(name, TagsFromVal(tags)).Set(value);
    }

    void ageGaugeSet(const std::string& name, double seconds, const val& tags)
    {
        reg.CreateAgeGauge(name, TagsFromVal(tags)).Set(seconds);
    }

    void ageGaugeNow(const std::string& name, const val& tags)
    {
        reg.CreateAgeGauge(name, TagsFromVal(tags)).Now();
    }

    void monotonicCounterSet(const std::string& name, double amount, const val& tags)
    {
        reg.CreateMonotonicCounter(name, TagsFromVal(tags)).Set(amount);
    }

    void monotonicCounterUintSet(const std::string& name, uint64_t amount, const val& tags)
    {
        reg.CreateMonotonicCounterUint(name, TagsFromVal(tags)).Set(amount);
    }

    void timerRecord(const std::string& name, double seconds, const val& tags)
    {
        reg.CreateTimer(name, TagsFromVal(tags)).Record(seconds);
    }

    void pctTimerRecord(const std::string& name, double seconds, const val& tags)
    {
        reg.CreatePercentTimer(name, TagsFromVal(tags)).Record(seconds);
    }

    void distRecord(const std::string& name, int64_t amount, const val& tags)
    {
        reg.CreateDistributionSummary(name, TagsFromVal(tags)).Record(amount);
    }

    void pctDistRecord(const std::string& name, int64_t amount, const val& tags)
    {
        reg.CreatePercentDistributionSummary(name, TagsFromVal(tags)).Record(amount);
    }

    // Returns all accumulated metric lines as a single string (each ending with \n).
    // The JS layer splits on '\n' and relays batches via fetch/WebSocket.
    std::string flush()
    {
        return spectator::Writer::DumpMemory();
    }
};

EMSCRIPTEN_BINDINGS(spectator)
{
    class_<WasmRegistry>("Registry")
        .constructor()
        .function("counterIncrement",        &WasmRegistry::counterIncrement)
        .function("counterAdd",              &WasmRegistry::counterAdd)
        .function("gaugeSet",                &WasmRegistry::gaugeSet)
        .function("gaugeSetTtl",             &WasmRegistry::gaugeSetTtl)
        .function("maxGaugeSet",             &WasmRegistry::maxGaugeSet)
        .function("ageGaugeSet",             &WasmRegistry::ageGaugeSet)
        .function("ageGaugeNow",             &WasmRegistry::ageGaugeNow)
        .function("monotonicCounterSet",     &WasmRegistry::monotonicCounterSet)
        .function("monotonicCounterUintSet", &WasmRegistry::monotonicCounterUintSet)
        .function("timerRecord",             &WasmRegistry::timerRecord)
        .function("pctTimerRecord",          &WasmRegistry::pctTimerRecord)
        .function("distRecord",              &WasmRegistry::distRecord)
        .function("pctDistRecord",           &WasmRegistry::pctDistRecord)
        .function("flush",                   &WasmRegistry::flush);
}
