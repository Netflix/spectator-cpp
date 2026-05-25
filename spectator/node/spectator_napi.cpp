#include <napi.h>

#include <age_gauge.h>
#include <counter.h>
#include <dist_summary.h>
#include <gauge.h>
#include <max_gauge.h>
#include <monotonic_counter.h>
#include <monotonic_counter_uint.h>
#include <percentile_dist_summary.h>
#include <percentile_timer.h>
#include <registry.h>
#include <timer.h>
#include <config.h>
#include <writer.h>
#include <writer_config.h>

#include <string>
#include <unordered_map>

namespace {

using Tags = std::unordered_map<std::string, std::string>;

Tags TagsFromJs(Napi::Env env, Napi::Value val)
{
    Tags tags;
    if (val.IsNull() || val.IsUndefined() || !val.IsObject()) return tags;
    Napi::Object obj  = val.As<Napi::Object>();
    Napi::Array  keys = obj.GetPropertyNames();
    tags.reserve(keys.Length());
    for (uint32_t i = 0; i < keys.Length(); ++i)
    {
        Napi::Value k = keys[i];
        Napi::Value v = obj.Get(k);
        if (k.IsString() && v.IsString())
            tags.emplace(k.As<Napi::String>().Utf8Value(),
                         v.As<Napi::String>().Utf8Value());
    }
    return tags;
}

std::string StrArg(const Napi::CallbackInfo& info, uint32_t idx)
{
    return info[idx].As<Napi::String>().Utf8Value();
}

// ---------------------------------------------------------------------------
// Macro — generates a persistent meter ObjectWrap<T> class.
// Each instance owns a heap-allocated C++ meter so m_line is reused.
// ---------------------------------------------------------------------------
#define METER_WRAP(ClassName, CppType, NapiName)                               \
class ClassName : public Napi::ObjectWrap<ClassName> {                         \
public:                                                                        \
    static Napi::Function Init(Napi::Env env, Napi::Object exports) {         \
        return DefineClass(env, NapiName, GetMethods());                       \
    }                                                                          \
    static Napi::FunctionReference& constructor() {                            \
        static Napi::FunctionReference ref;                                    \
        return ref;                                                             \
    }                                                                          \
    static std::vector<Napi::ClassPropertyDescriptor<ClassName>> GetMethods();\
    ClassName(const Napi::CallbackInfo& info)                                  \
        : Napi::ObjectWrap<ClassName>(info)                                    \
    {                                                                          \
        if (info[0].IsExternal())                                              \
            meter_ = info[0].As<Napi::External<CppType>>().Data();            \
    }                                                                          \
    ~ClassName() { delete meter_; }                                            \
    static Napi::Object Create(Napi::Env env, CppType* meter) {               \
        auto ext = Napi::External<CppType>::New(env, meter);                  \
        return constructor().New({ext});                                       \
    }                                                                          \
private:                                                                       \
    CppType* meter_ = nullptr;

} // namespace (re-opened in macro body below)

// ---------------------------------------------------------------------------
// Forward-declare all wrapper classes so RegistryWrap can reference them
// ---------------------------------------------------------------------------
class CounterWrap;
class GaugeWrap;
class MaxGaugeWrap;
class AgeGaugeWrap;
class MonotonicCounterWrap;
class MonotonicCounterUintWrap;
class TimerWrap;
class PercentileTimerWrap;
class DistributionSummaryWrap;
class PercentileDistributionSummaryWrap;

// ---------------------------------------------------------------------------
// CounterWrap
// ---------------------------------------------------------------------------
class CounterWrap : public Napi::ObjectWrap<CounterWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "Counter", {
            InstanceMethod("increment", &CounterWrap::Increment),
            InstanceMethod("add",       &CounterWrap::Add),
        });
        Ctor() = Napi::Persistent(fn);
        Ctor().SuppressDestruct();
    }
    CounterWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<CounterWrap>(info) {
        if (info[0].IsExternal())
            meter_ = info[0].As<Napi::External<spectator::Counter>>().Data();
    }
    ~CounterWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::Counter* m) {
        return Ctor().New({Napi::External<spectator::Counter>::New(env, m)});
    }
private:
    spectator::Counter* meter_ = nullptr;
    Napi::Value Increment(const Napi::CallbackInfo& info) {
        meter_->Increment(); return info.Env().Undefined();
    }
    Napi::Value Add(const Napi::CallbackInfo& info) {
        meter_->Increment(info[0].As<Napi::BigInt>().Int64Value(nullptr));
        return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// GaugeWrap
// ---------------------------------------------------------------------------
class GaugeWrap : public Napi::ObjectWrap<GaugeWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "Gauge", { InstanceMethod("set", &GaugeWrap::Set) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    GaugeWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<GaugeWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::Gauge>>().Data();
    }
    ~GaugeWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::Gauge* m) {
        return Ctor().New({Napi::External<spectator::Gauge>::New(env, m)});
    }
private:
    spectator::Gauge* meter_ = nullptr;
    Napi::Value Set(const Napi::CallbackInfo& info) {
        meter_->Set(info[0].As<Napi::Number>().DoubleValue()); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// MaxGaugeWrap
// ---------------------------------------------------------------------------
class MaxGaugeWrap : public Napi::ObjectWrap<MaxGaugeWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "MaxGauge", { InstanceMethod("set", &MaxGaugeWrap::Set) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    MaxGaugeWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<MaxGaugeWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::MaxGauge>>().Data();
    }
    ~MaxGaugeWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::MaxGauge* m) {
        return Ctor().New({Napi::External<spectator::MaxGauge>::New(env, m)});
    }
private:
    spectator::MaxGauge* meter_ = nullptr;
    Napi::Value Set(const Napi::CallbackInfo& info) {
        meter_->Set(info[0].As<Napi::Number>().DoubleValue()); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// AgeGaugeWrap
// ---------------------------------------------------------------------------
class AgeGaugeWrap : public Napi::ObjectWrap<AgeGaugeWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "AgeGauge", {
            InstanceMethod("set", &AgeGaugeWrap::Set),
            InstanceMethod("now", &AgeGaugeWrap::Now),
        });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    AgeGaugeWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<AgeGaugeWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::AgeGauge>>().Data();
    }
    ~AgeGaugeWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::AgeGauge* m) {
        return Ctor().New({Napi::External<spectator::AgeGauge>::New(env, m)});
    }
private:
    spectator::AgeGauge* meter_ = nullptr;
    Napi::Value Set(const Napi::CallbackInfo& info) {
        meter_->Set(info[0].As<Napi::Number>().DoubleValue()); return info.Env().Undefined();
    }
    Napi::Value Now(const Napi::CallbackInfo& info) {
        meter_->Now(); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// MonotonicCounterWrap
// ---------------------------------------------------------------------------
class MonotonicCounterWrap : public Napi::ObjectWrap<MonotonicCounterWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "MonotonicCounter", { InstanceMethod("set", &MonotonicCounterWrap::Set) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    MonotonicCounterWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<MonotonicCounterWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::MonotonicCounter>>().Data();
    }
    ~MonotonicCounterWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::MonotonicCounter* m) {
        return Ctor().New({Napi::External<spectator::MonotonicCounter>::New(env, m)});
    }
private:
    spectator::MonotonicCounter* meter_ = nullptr;
    Napi::Value Set(const Napi::CallbackInfo& info) {
        meter_->Set(info[0].As<Napi::Number>().DoubleValue()); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// MonotonicCounterUintWrap
// ---------------------------------------------------------------------------
class MonotonicCounterUintWrap : public Napi::ObjectWrap<MonotonicCounterUintWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "MonotonicCounterUint", { InstanceMethod("set", &MonotonicCounterUintWrap::Set) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    MonotonicCounterUintWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<MonotonicCounterUintWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::MonotonicCounterUint>>().Data();
    }
    ~MonotonicCounterUintWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::MonotonicCounterUint* m) {
        return Ctor().New({Napi::External<spectator::MonotonicCounterUint>::New(env, m)});
    }
private:
    spectator::MonotonicCounterUint* meter_ = nullptr;
    Napi::Value Set(const Napi::CallbackInfo& info) {
        meter_->Set(info[0].As<Napi::BigInt>().Uint64Value(nullptr)); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// TimerWrap
// ---------------------------------------------------------------------------
class TimerWrap : public Napi::ObjectWrap<TimerWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "Timer", { InstanceMethod("record", &TimerWrap::Record) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    TimerWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<TimerWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::Timer>>().Data();
    }
    ~TimerWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::Timer* m) {
        return Ctor().New({Napi::External<spectator::Timer>::New(env, m)});
    }
private:
    spectator::Timer* meter_ = nullptr;
    Napi::Value Record(const Napi::CallbackInfo& info) {
        meter_->Record(info[0].As<Napi::Number>().DoubleValue()); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// PercentileTimerWrap
// ---------------------------------------------------------------------------
class PercentileTimerWrap : public Napi::ObjectWrap<PercentileTimerWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "PercentileTimer", { InstanceMethod("record", &PercentileTimerWrap::Record) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    PercentileTimerWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<PercentileTimerWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::PercentileTimer>>().Data();
    }
    ~PercentileTimerWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::PercentileTimer* m) {
        return Ctor().New({Napi::External<spectator::PercentileTimer>::New(env, m)});
    }
private:
    spectator::PercentileTimer* meter_ = nullptr;
    Napi::Value Record(const Napi::CallbackInfo& info) {
        meter_->Record(info[0].As<Napi::Number>().DoubleValue()); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// DistributionSummaryWrap
// ---------------------------------------------------------------------------
class DistributionSummaryWrap : public Napi::ObjectWrap<DistributionSummaryWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "DistributionSummary", { InstanceMethod("record", &DistributionSummaryWrap::Record) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    DistributionSummaryWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<DistributionSummaryWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::DistributionSummary>>().Data();
    }
    ~DistributionSummaryWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::DistributionSummary* m) {
        return Ctor().New({Napi::External<spectator::DistributionSummary>::New(env, m)});
    }
private:
    spectator::DistributionSummary* meter_ = nullptr;
    Napi::Value Record(const Napi::CallbackInfo& info) {
        meter_->Record(info[0].As<Napi::BigInt>().Int64Value(nullptr)); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// PercentileDistributionSummaryWrap
// ---------------------------------------------------------------------------
class PercentileDistributionSummaryWrap : public Napi::ObjectWrap<PercentileDistributionSummaryWrap> {
public:
    static Napi::FunctionReference& Ctor() { static Napi::FunctionReference r; return r; }
    static void Init(Napi::Env env) {
        auto fn = DefineClass(env, "PercentileDistributionSummary", { InstanceMethod("record", &PercentileDistributionSummaryWrap::Record) });
        Ctor() = Napi::Persistent(fn); Ctor().SuppressDestruct();
    }
    PercentileDistributionSummaryWrap(const Napi::CallbackInfo& info) : Napi::ObjectWrap<PercentileDistributionSummaryWrap>(info) {
        if (info[0].IsExternal()) meter_ = info[0].As<Napi::External<spectator::PercentileDistributionSummary>>().Data();
    }
    ~PercentileDistributionSummaryWrap() { delete meter_; }
    static Napi::Object Create(Napi::Env env, spectator::PercentileDistributionSummary* m) {
        return Ctor().New({Napi::External<spectator::PercentileDistributionSummary>::New(env, m)});
    }
private:
    spectator::PercentileDistributionSummary* meter_ = nullptr;
    Napi::Value Record(const Napi::CallbackInfo& info) {
        meter_->Record(info[0].As<Napi::BigInt>().Int64Value(nullptr)); return info.Env().Undefined();
    }
};

// ---------------------------------------------------------------------------
// RegistryWrap — factory methods return persistent meter ObjectWrap objects
// ---------------------------------------------------------------------------
class RegistryWrap : public Napi::ObjectWrap<RegistryWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports)
    {
        Napi::Function fn = DefineClass(env, "Registry", {
            InstanceMethod("counter",               &RegistryWrap::Counter),
            InstanceMethod("gauge",                 &RegistryWrap::Gauge),
            InstanceMethod("gaugeTtl",              &RegistryWrap::GaugeTtl),
            InstanceMethod("maxGauge",              &RegistryWrap::MaxGauge),
            InstanceMethod("ageGauge",              &RegistryWrap::AgeGauge),
            InstanceMethod("monotonicCounter",      &RegistryWrap::MonotonicCounter),
            InstanceMethod("monotonicCounterUint",  &RegistryWrap::MonotonicCounterUint),
            InstanceMethod("timer",                 &RegistryWrap::TimerM),
            InstanceMethod("pctTimer",              &RegistryWrap::PctTimer),
            InstanceMethod("distSummary",           &RegistryWrap::DistSummary),
            InstanceMethod("pctDistSummary",        &RegistryWrap::PctDistSummary),
            InstanceMethod("dumpLines",             &RegistryWrap::DumpLines),
            InstanceMethod("clearWriter",           &RegistryWrap::ClearWriter),
        });
        exports.Set("Registry", fn);
        return fn;
    }

    RegistryWrap(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<RegistryWrap>(info)
    {
        std::string location = info.Length() > 0 && info[0].IsString()
            ? info[0].As<Napi::String>().Utf8Value() : "udp";
        unsigned int buf = info.Length() > 1 && info[1].IsNumber()
            ? info[1].As<Napi::Number>().Uint32Value() : 0u;
        spectator::WriterConfig wc = buf > 0
            ? spectator::WriterConfig(location, buf)
            : spectator::WriterConfig(location);
        registry_ = new spectator::Registry(spectator::Config(wc));
    }
    ~RegistryWrap() { delete registry_; }

private:
    spectator::Registry* registry_;

    spectator::MeterId id(const Napi::CallbackInfo& info, uint32_t nameIdx, uint32_t tagsIdx) {
        return registry_->CreateNewId(StrArg(info, nameIdx), TagsFromJs(info.Env(), info[tagsIdx]));
    }

    Napi::Value Counter(const Napi::CallbackInfo& info) {
        return CounterWrap::Create(info.Env(),
            new spectator::Counter(id(info, 0, 1)));
    }
    Napi::Value Gauge(const Napi::CallbackInfo& info) {
        return GaugeWrap::Create(info.Env(),
            new spectator::Gauge(id(info, 0, 1)));
    }
    Napi::Value GaugeTtl(const Napi::CallbackInfo& info) {
        int ttl = info[1].As<Napi::Number>().Int32Value();
        return GaugeWrap::Create(info.Env(),
            new spectator::Gauge(id(info, 0, 2), ttl));
    }
    Napi::Value MaxGauge(const Napi::CallbackInfo& info) {
        return MaxGaugeWrap::Create(info.Env(),
            new spectator::MaxGauge(id(info, 0, 1)));
    }
    Napi::Value AgeGauge(const Napi::CallbackInfo& info) {
        return AgeGaugeWrap::Create(info.Env(),
            new spectator::AgeGauge(id(info, 0, 1)));
    }
    Napi::Value MonotonicCounter(const Napi::CallbackInfo& info) {
        return MonotonicCounterWrap::Create(info.Env(),
            new spectator::MonotonicCounter(id(info, 0, 1)));
    }
    Napi::Value MonotonicCounterUint(const Napi::CallbackInfo& info) {
        return MonotonicCounterUintWrap::Create(info.Env(),
            new spectator::MonotonicCounterUint(id(info, 0, 1)));
    }
    Napi::Value TimerM(const Napi::CallbackInfo& info) {
        return TimerWrap::Create(info.Env(),
            new spectator::Timer(id(info, 0, 1)));
    }
    Napi::Value PctTimer(const Napi::CallbackInfo& info) {
        return PercentileTimerWrap::Create(info.Env(),
            new spectator::PercentileTimer(id(info, 0, 1)));
    }
    Napi::Value DistSummary(const Napi::CallbackInfo& info) {
        return DistributionSummaryWrap::Create(info.Env(),
            new spectator::DistributionSummary(id(info, 0, 1)));
    }
    Napi::Value PctDistSummary(const Napi::CallbackInfo& info) {
        return PercentileDistributionSummaryWrap::Create(info.Env(),
            new spectator::PercentileDistributionSummary(id(info, 0, 1)));
    }

    Napi::Value DumpLines(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        std::string dump = spectator::Writer::DumpMemory();
        Napi::Array result = Napi::Array::New(env);
        uint32_t idx = 0;
        size_t start = 0;
        while (start < dump.size())
        {
            size_t end = dump.find('\n', start);
            if (end == std::string::npos) end = dump.size();
            if (end > start)
                result[idx++] = Napi::String::New(env, dump.substr(start, end - start));
            start = end + 1;
        }
        return result;
    }

    Napi::Value ClearWriter(const Napi::CallbackInfo& info)
    {
        spectator::Writer::ClearMemory();
        return info.Env().Undefined();
    }
};

NODE_API_MODULE(spectator_cpp, [](Napi::Env env, Napi::Object exports) {
    // Init all meter wrap classes first so their constructors are registered
    CounterWrap::Init(env);
    GaugeWrap::Init(env);
    MaxGaugeWrap::Init(env);
    AgeGaugeWrap::Init(env);
    MonotonicCounterWrap::Init(env);
    MonotonicCounterUintWrap::Init(env);
    TimerWrap::Init(env);
    PercentileTimerWrap::Init(env);
    DistributionSummaryWrap::Init(env);
    PercentileDistributionSummaryWrap::Init(env);

    RegistryWrap::Init(env, exports);
})
