#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle to a spectator Registry.
 * Created via spectator_registry_new(), destroyed via spectator_registry_destroy().
 */
typedef void* spectator_registry_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/**
 * Create a new Registry.
 *
 * @param writer_location  Output location string: "udp", "udp://host:port",
 *                         "unix", "unix:///path", "memory", "noop".
 *                         NULL or "" defaults to "udp".
 * @param buffer_size      Bytes to buffer before flushing to the socket.
 *                         0 = unbuffered (send every metric immediately).
 *                         Use 60*1024 for high-throughput scenarios.
 */
spectator_registry_t spectator_registry_new(const char* writer_location,
                                             unsigned int buffer_size);

/**
 * Destroy a Registry and release all associated resources.
 * The handle must not be used after this call.
 */
void spectator_registry_destroy(spectator_registry_t registry);

/* --------------------------------------------------------------------------
 * Tag convention:
 *   All meter functions accept tags as two parallel C-string arrays:
 *     tag_keys[0..num_tags-1]   — tag key strings
 *     tag_vals[0..num_tags-1]   — tag value strings
 *   Pass NULL / 0 for num_tags when there are no extra tags.
 * -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 * Counter — monotonically increasing integer count
 * -------------------------------------------------------------------------- */

void spectator_counter_increment(spectator_registry_t registry,
                                  const char* name,
                                  const char** tag_keys,
                                  const char** tag_vals,
                                  int num_tags);

void spectator_counter_add(spectator_registry_t registry,
                            const char* name,
                            int64_t delta,
                            const char** tag_keys,
                            const char** tag_vals,
                            int num_tags);

/* --------------------------------------------------------------------------
 * Gauge — current value at point in time
 * -------------------------------------------------------------------------- */

void spectator_gauge_set(spectator_registry_t registry,
                          const char* name,
                          double value,
                          const char** tag_keys,
                          const char** tag_vals,
                          int num_tags);

void spectator_gauge_set_ttl(spectator_registry_t registry,
                               const char* name,
                               double value,
                               int ttl_seconds,
                               const char** tag_keys,
                               const char** tag_vals,
                               int num_tags);

/* --------------------------------------------------------------------------
 * MaxGauge — tracks the maximum value seen
 * -------------------------------------------------------------------------- */

void spectator_max_gauge_set(spectator_registry_t registry,
                               const char* name,
                               double value,
                               const char** tag_keys,
                               const char** tag_vals,
                               int num_tags);

/* --------------------------------------------------------------------------
 * AgeGauge — tracks time since last update (seconds)
 * -------------------------------------------------------------------------- */

void spectator_age_gauge_set(spectator_registry_t registry,
                               const char* name,
                               double seconds,
                               const char** tag_keys,
                               const char** tag_vals,
                               int num_tags);

void spectator_age_gauge_now(spectator_registry_t registry,
                               const char* name,
                               const char** tag_keys,
                               const char** tag_vals,
                               int num_tags);

/* --------------------------------------------------------------------------
 * MonotonicCounter — monotonically increasing double (rate reported)
 * -------------------------------------------------------------------------- */

void spectator_monotonic_counter_set(spectator_registry_t registry,
                                      const char* name,
                                      double amount,
                                      const char** tag_keys,
                                      const char** tag_vals,
                                      int num_tags);

/* --------------------------------------------------------------------------
 * MonotonicCounterUint — monotonically increasing uint64
 * -------------------------------------------------------------------------- */

void spectator_monotonic_counter_uint_set(spectator_registry_t registry,
                                           const char* name,
                                           uint64_t amount,
                                           const char** tag_keys,
                                           const char** tag_vals,
                                           int num_tags);

/* --------------------------------------------------------------------------
 * Timer — records durations in seconds
 * -------------------------------------------------------------------------- */

void spectator_timer_record(spectator_registry_t registry,
                              const char* name,
                              double seconds,
                              const char** tag_keys,
                              const char** tag_vals,
                              int num_tags);

/* --------------------------------------------------------------------------
 * PercentileTimer — timer with percentile bucketing
 * -------------------------------------------------------------------------- */

void spectator_pct_timer_record(spectator_registry_t registry,
                                  const char* name,
                                  double seconds,
                                  const char** tag_keys,
                                  const char** tag_vals,
                                  int num_tags);

/* --------------------------------------------------------------------------
 * DistributionSummary — records sizes / amounts (int64)
 * -------------------------------------------------------------------------- */

void spectator_dist_record(spectator_registry_t registry,
                             const char* name,
                             int64_t amount,
                             const char** tag_keys,
                             const char** tag_vals,
                             int num_tags);

/* --------------------------------------------------------------------------
 * PercentileDistributionSummary — distribution with percentile bucketing
 * -------------------------------------------------------------------------- */

void spectator_pct_dist_record(spectator_registry_t registry,
                                 const char* name,
                                 int64_t amount,
                                 const char** tag_keys,
                                 const char** tag_vals,
                                 int num_tags);

/* --------------------------------------------------------------------------
 * Memory writer utilities
 * Only meaningful when the registry was created with writer_location "memory".
 * -------------------------------------------------------------------------- */

/**
 * Return all accumulated lines from the memory writer as a single
 * heap-allocated C string. Each line already ends with '\n'.
 * Returns an empty string (not NULL) if the writer is not a MemoryWriter.
 * The caller must free the returned pointer with spectator_free_string().
 */
char* spectator_memory_writer_dump(spectator_registry_t registry);

/**
 * Free a string returned by spectator_memory_writer_dump().
 */
void spectator_free_string(char* s);

#ifdef __cplusplus
}
#endif
