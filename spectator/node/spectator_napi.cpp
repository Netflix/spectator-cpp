#include <napi.h>

#include <registry.h>
#include <config.h>
#include <writer.h>
#include <writer_config.h>

#include <string>
#include <unordered_map>

namespace {

using Tags = std::unordered_map<std::string, std::string>;

// Convert a plain JS object {key: "value", ...} to a C++ tags map.
Tags TagsFromJs(Napi::Env env, Napi::Value val)
{
    Tags tags;
    if (val.IsNull() || val.IsUndefined()) return tags;
    if (!val.IsObject()) return tags;

    Napi::Object obj = val.As<Napi::Object>();
    Napi::Array keys = obj.GetPropertyNames();
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

std::string StringArg(const Napi::CallbackInfo& info, uint32_t idx)
{
    return info[idx].As<Napi::String>().Utf8Value();
}

} // namespace

// ---------------------------------------------------------------------------
// RegistryWrap — wraps spectator::Registry as a Node.js object
// ---------------------------------------------------------------------------

class RegistryWrap : public Napi::ObjectWrap<RegistryWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports)
    {
        Napi::Function func = DefineClass(env, "Registry", {
            InstanceMethod("counterIncrement",        &RegistryWrap::CounterIncrement),
            InstanceMethod("counterAdd",              &RegistryWrap::CounterAdd),
            InstanceMethod("gaugeSet",                &RegistryWrap::GaugeSet),
            InstanceMethod("gaugeSetTtl",             &RegistryWrap::GaugeSetTtl),
            InstanceMethod("maxGaugeSet",             &RegistryWrap::MaxGaugeSet),
            InstanceMethod("ageGaugeSet",             &RegistryWrap::AgeGaugeSet),
            InstanceMethod("ageGaugeNow",             &RegistryWrap::AgeGaugeNow),
            InstanceMethod("monotonicCounterSet",     &RegistryWrap::MonotonicCounterSet),
            InstanceMethod("monotonicCounterUintSet", &RegistryWrap::MonotonicCounterUintSet),
            InstanceMethod("timerRecord",             &RegistryWrap::TimerRecord),
            InstanceMethod("pctTimerRecord",          &RegistryWrap::PctTimerRecord),
            InstanceMethod("distRecord",              &RegistryWrap::DistRecord),
            InstanceMethod("pctDistRecord",           &RegistryWrap::PctDistRecord),
            InstanceMethod("dumpLines",               &RegistryWrap::DumpLines),
            InstanceMethod("clearWriter",             &RegistryWrap::ClearWriter),
        });
        exports.Set("Registry", func);
        return func;
    }

    // new Registry(location = "udp", bufferSize = 0)
    RegistryWrap(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<RegistryWrap>(info)
    {
        std::string location = info.Length() > 0 && info[0].IsString()
            ? info[0].As<Napi::String>().Utf8Value() : "udp";
        unsigned int bufferSize = info.Length() > 1 && info[1].IsNumber()
            ? info[1].As<Napi::Number>().Uint32Value() : 0u;

        spectator::WriterConfig wc = bufferSize > 0
            ? spectator::WriterConfig(location, bufferSize)
            : spectator::WriterConfig(location);
        registry_ = new spectator::Registry(spectator::Config(wc));
    }

    ~RegistryWrap() { delete registry_; }

private:
    spectator::Registry* registry_;

    // counterIncrement(name: string, tags?: object)
    Napi::Value CounterIncrement(const Napi::CallbackInfo& info)
    {
        registry_->CreateCounter(StringArg(info, 0),
                                  TagsFromJs(info.Env(), info[1])).Increment();
        return info.Env().Undefined();
    }

    // counterAdd(name: string, delta: bigint, tags?: object)
    Napi::Value CounterAdd(const Napi::CallbackInfo& info)
    {
        int64_t delta = info[1].As<Napi::BigInt>().Int64Value(nullptr);
        registry_->CreateCounter(StringArg(info, 0),
                                  TagsFromJs(info.Env(), info[2])).Increment(delta);
        return info.Env().Undefined();
    }

    // gaugeSet(name: string, value: number, tags?: object)
    Napi::Value GaugeSet(const Napi::CallbackInfo& info)
    {
        registry_->CreateGauge(StringArg(info, 0),
                                TagsFromJs(info.Env(), info[2]))
            .Set(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    // gaugeSetTtl(name, value, ttlSeconds, tags?)
    Napi::Value GaugeSetTtl(const Napi::CallbackInfo& info)
    {
        int ttl = info[2].As<Napi::Number>().Int32Value();
        registry_->CreateGauge(StringArg(info, 0),
                                TagsFromJs(info.Env(), info[3]), ttl)
            .Set(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    Napi::Value MaxGaugeSet(const Napi::CallbackInfo& info)
    {
        registry_->CreateMaxGauge(StringArg(info, 0),
                                   TagsFromJs(info.Env(), info[2]))
            .Set(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    Napi::Value AgeGaugeSet(const Napi::CallbackInfo& info)
    {
        registry_->CreateAgeGauge(StringArg(info, 0),
                                   TagsFromJs(info.Env(), info[2]))
            .Set(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    Napi::Value AgeGaugeNow(const Napi::CallbackInfo& info)
    {
        registry_->CreateAgeGauge(StringArg(info, 0),
                                   TagsFromJs(info.Env(), info[1])).Now();
        return info.Env().Undefined();
    }

    Napi::Value MonotonicCounterSet(const Napi::CallbackInfo& info)
    {
        registry_->CreateMonotonicCounter(StringArg(info, 0),
                                           TagsFromJs(info.Env(), info[2]))
            .Set(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    Napi::Value MonotonicCounterUintSet(const Napi::CallbackInfo& info)
    {
        uint64_t amount = info[1].As<Napi::BigInt>().Uint64Value(nullptr);
        registry_->CreateMonotonicCounterUint(StringArg(info, 0),
                                               TagsFromJs(info.Env(), info[2]))
            .Set(amount);
        return info.Env().Undefined();
    }

    // timerRecord(name, seconds: number, tags?)
    Napi::Value TimerRecord(const Napi::CallbackInfo& info)
    {
        registry_->CreateTimer(StringArg(info, 0),
                                TagsFromJs(info.Env(), info[2]))
            .Record(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    Napi::Value PctTimerRecord(const Napi::CallbackInfo& info)
    {
        registry_->CreatePercentTimer(StringArg(info, 0),
                                       TagsFromJs(info.Env(), info[2]))
            .Record(info[1].As<Napi::Number>().DoubleValue());
        return info.Env().Undefined();
    }

    // distRecord(name, amount: bigint, tags?)
    Napi::Value DistRecord(const Napi::CallbackInfo& info)
    {
        int64_t amount = info[1].As<Napi::BigInt>().Int64Value(nullptr);
        registry_->CreateDistributionSummary(StringArg(info, 0),
                                              TagsFromJs(info.Env(), info[2]))
            .Record(amount);
        return info.Env().Undefined();
    }

    Napi::Value PctDistRecord(const Napi::CallbackInfo& info)
    {
        int64_t amount = info[1].As<Napi::BigInt>().Int64Value(nullptr);
        registry_->CreatePercentDistributionSummary(StringArg(info, 0),
                                                     TagsFromJs(info.Env(), info[2]))
            .Record(amount);
        return info.Env().Undefined();
    }

    // dumpLines() → string[] — only meaningful with memory writer
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

    // clearWriter() — clears the memory writer buffer
    Napi::Value ClearWriter(const Napi::CallbackInfo& info)
    {
        spectator::Writer::ClearMemory();
        return info.Env().Undefined();
    }
};

NODE_API_MODULE(spectator_cpp, [](Napi::Env env, Napi::Object exports) {
    RegistryWrap::Init(env, exports);
})
