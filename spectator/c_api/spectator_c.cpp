#include "spectator_c.h"

#include <registry.h>
#include <config.h>
#include <writer.h>
#include <writer_config.h>

#include <cstdlib>
#include <cstring>

#include <string>
#include <unordered_map>

namespace {

using Tags = std::unordered_map<std::string, std::string>;

Tags make_tags(const char** keys, const char** vals, int n)
{
    Tags tags;
    if (n <= 0 || keys == nullptr || vals == nullptr) return tags;
    tags.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        if (keys[i] && vals[i])
            tags.emplace(keys[i], vals[i]);
    }
    return tags;
}

inline spectator::Registry* reg(spectator_registry_t r)
{
    return static_cast<spectator::Registry*>(r);
}

}  // namespace

extern "C" {

spectator_registry_t spectator_registry_new(const char* writer_location,
                                             unsigned int buffer_size)
{
    const std::string location = (writer_location && *writer_location) ? writer_location : "udp";
    spectator::WriterConfig wc = buffer_size > 0
        ? spectator::WriterConfig(location, buffer_size)
        : spectator::WriterConfig(location);
    return new spectator::Registry(spectator::Config(wc));
}

void spectator_registry_destroy(spectator_registry_t registry)
{
    delete static_cast<spectator::Registry*>(registry);
}

// Counter

void spectator_counter_increment(spectator_registry_t r,
                                  const char* name,
                                  const char** keys, const char** vals, int n)
{
    reg(r)->CreateCounter(name, make_tags(keys, vals, n)).Increment();
}

void spectator_counter_add(spectator_registry_t r,
                            const char* name, int64_t delta,
                            const char** keys, const char** vals, int n)
{
    reg(r)->CreateCounter(name, make_tags(keys, vals, n)).Increment(delta);
}

// Gauge

void spectator_gauge_set(spectator_registry_t r,
                          const char* name, double value,
                          const char** keys, const char** vals, int n)
{
    reg(r)->CreateGauge(name, make_tags(keys, vals, n)).Set(value);
}

void spectator_gauge_set_ttl(spectator_registry_t r,
                               const char* name, double value, int ttl_seconds,
                               const char** keys, const char** vals, int n)
{
    reg(r)->CreateGauge(name, make_tags(keys, vals, n), ttl_seconds).Set(value);
}

// MaxGauge

void spectator_max_gauge_set(spectator_registry_t r,
                               const char* name, double value,
                               const char** keys, const char** vals, int n)
{
    reg(r)->CreateMaxGauge(name, make_tags(keys, vals, n)).Set(value);
}

// AgeGauge

void spectator_age_gauge_set(spectator_registry_t r,
                               const char* name, double seconds,
                               const char** keys, const char** vals, int n)
{
    reg(r)->CreateAgeGauge(name, make_tags(keys, vals, n)).Set(seconds);
}

void spectator_age_gauge_now(spectator_registry_t r,
                               const char* name,
                               const char** keys, const char** vals, int n)
{
    reg(r)->CreateAgeGauge(name, make_tags(keys, vals, n)).Now();
}

// MonotonicCounter

void spectator_monotonic_counter_set(spectator_registry_t r,
                                      const char* name, double amount,
                                      const char** keys, const char** vals, int n)
{
    reg(r)->CreateMonotonicCounter(name, make_tags(keys, vals, n)).Set(amount);
}

// MonotonicCounterUint

void spectator_monotonic_counter_uint_set(spectator_registry_t r,
                                           const char* name, uint64_t amount,
                                           const char** keys, const char** vals, int n)
{
    reg(r)->CreateMonotonicCounterUint(name, make_tags(keys, vals, n)).Set(amount);
}

// Timer

void spectator_timer_record(spectator_registry_t r,
                              const char* name, double seconds,
                              const char** keys, const char** vals, int n)
{
    reg(r)->CreateTimer(name, make_tags(keys, vals, n)).Record(seconds);
}

// PercentileTimer

void spectator_pct_timer_record(spectator_registry_t r,
                                  const char* name, double seconds,
                                  const char** keys, const char** vals, int n)
{
    reg(r)->CreatePercentTimer(name, make_tags(keys, vals, n)).Record(seconds);
}

// DistributionSummary

void spectator_dist_record(spectator_registry_t r,
                             const char* name, int64_t amount,
                             const char** keys, const char** vals, int n)
{
    reg(r)->CreateDistributionSummary(name, make_tags(keys, vals, n)).Record(amount);
}

// PercentileDistributionSummary

void spectator_pct_dist_record(spectator_registry_t r,
                                 const char* name, int64_t amount,
                                 const char** keys, const char** vals, int n)
{
    reg(r)->CreatePercentDistributionSummary(name, make_tags(keys, vals, n)).Record(amount);
}

// Memory writer

char* spectator_memory_writer_dump(spectator_registry_t /* registry */)
{
    std::string dump = spectator::Writer::DumpMemory();
    char* result = static_cast<char*>(malloc(dump.size() + 1));
    memcpy(result, dump.c_str(), dump.size() + 1);
    return result;
}

void spectator_free_string(char* s)
{
    free(s);
}

}  // extern "C"
