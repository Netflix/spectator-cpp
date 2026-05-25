#include "spectator_c.h"

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

// ==========================================================================
// Persistent meter handles — m_line is allocated once and reused every call
// ==========================================================================

// Counter
spectator_counter_h spectator_counter_new(spectator_registry_t r, const char* name,
                                           const char** keys, const char** vals, int n)
{
    return new spectator::Counter(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_counter_handle_destroy(spectator_counter_h h) { delete static_cast<spectator::Counter*>(h); }
void spectator_counter_handle_increment(spectator_counter_h h) { static_cast<spectator::Counter*>(h)->Increment(); }
void spectator_counter_handle_add(spectator_counter_h h, int64_t d) { static_cast<spectator::Counter*>(h)->Increment(d); }

// Gauge
spectator_gauge_h spectator_gauge_new(spectator_registry_t r, const char* name, int ttl,
                                       const char** keys, const char** vals, int n)
{
    auto tags = make_tags(keys, vals, n);
    auto id   = reg(r)->CreateNewId(name, tags);
    return ttl >= 0
        ? new spectator::Gauge(std::move(id), ttl)
        : new spectator::Gauge(std::move(id));
}
void spectator_gauge_handle_destroy(spectator_gauge_h h) { delete static_cast<spectator::Gauge*>(h); }
void spectator_gauge_handle_set(spectator_gauge_h h, double v) { static_cast<spectator::Gauge*>(h)->Set(v); }

// MaxGauge
spectator_max_gauge_h spectator_max_gauge_new(spectator_registry_t r, const char* name,
                                               const char** keys, const char** vals, int n)
{
    return new spectator::MaxGauge(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_max_gauge_handle_destroy(spectator_max_gauge_h h) { delete static_cast<spectator::MaxGauge*>(h); }
void spectator_max_gauge_handle_set(spectator_max_gauge_h h, double v) { static_cast<spectator::MaxGauge*>(h)->Set(v); }

// AgeGauge
spectator_age_gauge_h spectator_age_gauge_new(spectator_registry_t r, const char* name,
                                               const char** keys, const char** vals, int n)
{
    return new spectator::AgeGauge(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_age_gauge_handle_destroy(spectator_age_gauge_h h) { delete static_cast<spectator::AgeGauge*>(h); }
void spectator_age_gauge_handle_set(spectator_age_gauge_h h, double s) { static_cast<spectator::AgeGauge*>(h)->Set(s); }
void spectator_age_gauge_handle_now(spectator_age_gauge_h h) { static_cast<spectator::AgeGauge*>(h)->Now(); }

// MonotonicCounter
spectator_monotonic_counter_h spectator_monotonic_counter_new(spectator_registry_t r, const char* name,
                                                               const char** keys, const char** vals, int n)
{
    return new spectator::MonotonicCounter(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_monotonic_counter_handle_destroy(spectator_monotonic_counter_h h) { delete static_cast<spectator::MonotonicCounter*>(h); }
void spectator_monotonic_counter_handle_set(spectator_monotonic_counter_h h, double v) { static_cast<spectator::MonotonicCounter*>(h)->Set(v); }

// MonotonicCounterUint
spectator_monotonic_counter_uint_h spectator_monotonic_counter_uint_new(spectator_registry_t r, const char* name,
                                                                          const char** keys, const char** vals, int n)
{
    return new spectator::MonotonicCounterUint(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_monotonic_counter_uint_handle_destroy(spectator_monotonic_counter_uint_h h) { delete static_cast<spectator::MonotonicCounterUint*>(h); }
void spectator_monotonic_counter_uint_handle_set(spectator_monotonic_counter_uint_h h, uint64_t v) { static_cast<spectator::MonotonicCounterUint*>(h)->Set(v); }

// Timer
spectator_timer_h spectator_timer_new(spectator_registry_t r, const char* name,
                                       const char** keys, const char** vals, int n)
{
    return new spectator::Timer(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_timer_handle_destroy(spectator_timer_h h) { delete static_cast<spectator::Timer*>(h); }
void spectator_timer_handle_record(spectator_timer_h h, double s) { static_cast<spectator::Timer*>(h)->Record(s); }

// PercentileTimer
spectator_pct_timer_h spectator_pct_timer_new(spectator_registry_t r, const char* name,
                                               const char** keys, const char** vals, int n)
{
    return new spectator::PercentileTimer(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_pct_timer_handle_destroy(spectator_pct_timer_h h) { delete static_cast<spectator::PercentileTimer*>(h); }
void spectator_pct_timer_handle_record(spectator_pct_timer_h h, double s) { static_cast<spectator::PercentileTimer*>(h)->Record(s); }

// DistributionSummary
spectator_dist_h spectator_dist_new(spectator_registry_t r, const char* name,
                                     const char** keys, const char** vals, int n)
{
    return new spectator::DistributionSummary(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_dist_handle_destroy(spectator_dist_h h) { delete static_cast<spectator::DistributionSummary*>(h); }
void spectator_dist_handle_record(spectator_dist_h h, int64_t v) { static_cast<spectator::DistributionSummary*>(h)->Record(v); }

// PercentileDistributionSummary
spectator_pct_dist_h spectator_pct_dist_new(spectator_registry_t r, const char* name,
                                              const char** keys, const char** vals, int n)
{
    return new spectator::PercentileDistributionSummary(reg(r)->CreateNewId(name, make_tags(keys, vals, n)));
}
void spectator_pct_dist_handle_destroy(spectator_pct_dist_h h) { delete static_cast<spectator::PercentileDistributionSummary*>(h); }
void spectator_pct_dist_handle_record(spectator_pct_dist_h h, int64_t v) { static_cast<spectator::PercentileDistributionSummary*>(h)->Record(v); }

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
