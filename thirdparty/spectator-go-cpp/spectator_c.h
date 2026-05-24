// Copy of spectator/c_api/spectator_c.h — kept here so CGo can find it
// without requiring the C++ source tree at build time.
// Update this file when spectator_c.h changes in the parent repo.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* spectator_registry_t;

spectator_registry_t spectator_registry_new(const char* writer_location, unsigned int buffer_size);
void                 spectator_registry_destroy(spectator_registry_t registry);

void spectator_counter_increment(spectator_registry_t r, const char* name, const char** keys, const char** vals, int n);
void spectator_counter_add      (spectator_registry_t r, const char* name, int64_t delta, const char** keys, const char** vals, int n);

void spectator_gauge_set    (spectator_registry_t r, const char* name, double v, const char** keys, const char** vals, int n);
void spectator_gauge_set_ttl(spectator_registry_t r, const char* name, double v, int ttl, const char** keys, const char** vals, int n);

void spectator_max_gauge_set(spectator_registry_t r, const char* name, double v, const char** keys, const char** vals, int n);

void spectator_age_gauge_set(spectator_registry_t r, const char* name, double seconds, const char** keys, const char** vals, int n);
void spectator_age_gauge_now(spectator_registry_t r, const char* name, const char** keys, const char** vals, int n);

void spectator_monotonic_counter_set     (spectator_registry_t r, const char* name, double v,    const char** keys, const char** vals, int n);
void spectator_monotonic_counter_uint_set(spectator_registry_t r, const char* name, uint64_t v,  const char** keys, const char** vals, int n);

void spectator_timer_record    (spectator_registry_t r, const char* name, double s,      const char** keys, const char** vals, int n);
void spectator_pct_timer_record(spectator_registry_t r, const char* name, double s,      const char** keys, const char** vals, int n);

void spectator_dist_record    (spectator_registry_t r, const char* name, int64_t amount, const char** keys, const char** vals, int n);
void spectator_pct_dist_record(spectator_registry_t r, const char* name, int64_t amount, const char** keys, const char** vals, int n);

char*  spectator_memory_writer_dump(spectator_registry_t registry);
void   spectator_free_string(char* s);

#ifdef __cplusplus
}
#endif
