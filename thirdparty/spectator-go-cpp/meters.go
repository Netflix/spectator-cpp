package spectator

/*
#include "spectator_c.h"
#include <stdlib.h>
*/
import "C"
import "unsafe"

// -- Counter --

// Counter is a monotonically increasing integer counter.
type Counter struct {
	r    *Registry
	name string
	tags map[string]string
}

// Increment adds 1 to the counter.
func (c *Counter) Increment() {
	n := C.CString(c.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(c.tags)
	defer freeCArrays(k, v, count)
	C.spectator_counter_increment(c.r.ptr, n, k, v, count)
}

// Add adds delta to the counter (must be > 0).
func (c *Counter) Add(delta int64) {
	n := C.CString(c.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(c.tags)
	defer freeCArrays(k, v, count)
	C.spectator_counter_add(c.r.ptr, n, C.int64_t(delta), k, v, count)
}

// -- Gauge --

// Gauge records the current value at a point in time.
type Gauge struct {
	r    *Registry
	name string
	tags map[string]string
	ttl  int // -1 means no TTL
}

// Set sets the gauge to value.
func (g *Gauge) Set(value float64) {
	n := C.CString(g.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(g.tags)
	defer freeCArrays(k, v, count)
	if g.ttl >= 0 {
		C.spectator_gauge_set_ttl(g.r.ptr, n, C.double(value), C.int(g.ttl), k, v, count)
	} else {
		C.spectator_gauge_set(g.r.ptr, n, C.double(value), k, v, count)
	}
}

// -- MaxGauge --

// MaxGauge tracks the maximum value seen.
type MaxGauge struct {
	r    *Registry
	name string
	tags map[string]string
}

// Set updates the max-gauge if value is greater than the current maximum.
func (g *MaxGauge) Set(value float64) {
	n := C.CString(g.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(g.tags)
	defer freeCArrays(k, v, count)
	C.spectator_max_gauge_set(g.r.ptr, n, C.double(value), k, v, count)
}

// -- AgeGauge --

// AgeGauge tracks the time in seconds since the last event.
type AgeGauge struct {
	r    *Registry
	name string
	tags map[string]string
}

// Set records the given age in seconds.
func (a *AgeGauge) Set(seconds float64) {
	n := C.CString(a.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(a.tags)
	defer freeCArrays(k, v, count)
	C.spectator_age_gauge_set(a.r.ptr, n, C.double(seconds), k, v, count)
}

// Now records the current wall-clock time as the most recent event.
func (a *AgeGauge) Now() {
	n := C.CString(a.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(a.tags)
	defer freeCArrays(k, v, count)
	C.spectator_age_gauge_now(a.r.ptr, n, k, v, count)
}

// -- MonotonicCounter --

// MonotonicCounter tracks a monotonically increasing double value.
type MonotonicCounter struct {
	r    *Registry
	name string
	tags map[string]string
}

// Set updates the monotonic counter to amount.
func (m *MonotonicCounter) Set(amount float64) {
	n := C.CString(m.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(m.tags)
	defer freeCArrays(k, v, count)
	C.spectator_monotonic_counter_set(m.r.ptr, n, C.double(amount), k, v, count)
}

// -- MonotonicCounterUint --

// MonotonicCounterUint tracks a monotonically increasing uint64 value.
type MonotonicCounterUint struct {
	r    *Registry
	name string
	tags map[string]string
}

// Set updates the monotonic uint counter to amount.
func (m *MonotonicCounterUint) Set(amount uint64) {
	n := C.CString(m.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(m.tags)
	defer freeCArrays(k, v, count)
	C.spectator_monotonic_counter_uint_set(m.r.ptr, n, C.uint64_t(amount), k, v, count)
}

// -- Timer --

// Timer records durations in seconds.
type Timer struct {
	r    *Registry
	name string
	tags map[string]string
}

// Record records a duration in seconds.
func (t *Timer) Record(seconds float64) {
	n := C.CString(t.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(t.tags)
	defer freeCArrays(k, v, count)
	C.spectator_timer_record(t.r.ptr, n, C.double(seconds), k, v, count)
}

// -- PercentileTimer --

// PercentileTimer records durations with percentile bucketing.
type PercentileTimer struct {
	r    *Registry
	name string
	tags map[string]string
}

// Record records a duration in seconds.
func (t *PercentileTimer) Record(seconds float64) {
	n := C.CString(t.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(t.tags)
	defer freeCArrays(k, v, count)
	C.spectator_pct_timer_record(t.r.ptr, n, C.double(seconds), k, v, count)
}

// -- DistributionSummary --

// DistributionSummary records sizes or amounts as a distribution.
type DistributionSummary struct {
	r    *Registry
	name string
	tags map[string]string
}

// Record records amount.
func (d *DistributionSummary) Record(amount int64) {
	n := C.CString(d.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(d.tags)
	defer freeCArrays(k, v, count)
	C.spectator_dist_record(d.r.ptr, n, C.int64_t(amount), k, v, count)
}

// -- PercentileDistributionSummary --

// PercentileDistributionSummary records sizes with percentile bucketing.
type PercentileDistributionSummary struct {
	r    *Registry
	name string
	tags map[string]string
}

// Record records amount.
func (d *PercentileDistributionSummary) Record(amount int64) {
	n := C.CString(d.name)
	defer C.free(unsafe.Pointer(n))
	k, v, count := toCArrays(d.tags)
	defer freeCArrays(k, v, count)
	C.spectator_pct_dist_record(d.r.ptr, n, C.int64_t(amount), k, v, count)
}
