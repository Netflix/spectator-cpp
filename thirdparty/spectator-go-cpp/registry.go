// Package spectator provides Go bindings for spectator-cpp, the high-performance
// Netflix metrics client. All metric logic runs inside the C++ shared library
// (libspectator.so) via CGo, eliminating duplicate ID construction, sanitization,
// and wire-format code.
//
// Build requirements:
//   - libspectator.so must be on the linker path (set CGO_LDFLAGS or LD_LIBRARY_PATH)
//   - spectator_c.h must be present in this directory (copy from spectator/c_api/)
//
// Usage:
//
//	r := spectator.NewRegistry("udp", 0)
//	defer r.Close()
//	r.Counter("requests", map[string]string{"status": "200"}).Increment()
package spectator

/*
// Link the spectator-cpp static archive and its dependencies.
// Set SPECTATOR_LIB_DIR before building:
//   export SPECTATOR_LIB_DIR=/path/to/cmake-build/spectator
//
// Then build with:
//   CGO_LDFLAGS="-L${SPECTATOR_LIB_DIR} -lspectator_c -lboost_system -static-libstdc++ -static-libgcc" go build
//
// The resulting binary has zero C++ runtime .so dependencies.
#cgo LDFLAGS: -lstdc++ -lm -lpthread
#include "spectator_c.h"
#include <stdlib.h>
*/
import "C"
import (
	"runtime"
	"unsafe"
)

// Registry is the main entry point for recording metrics.
// It is safe for concurrent use from multiple goroutines.
type Registry struct {
	ptr C.spectator_registry_t
}

// NewRegistry creates a Registry backed by libspectator.so.
//
// writerLocation: "udp", "udp://host:port", "unix", "unix:///path",
//                 "memory", "noop". Empty string defaults to "udp".
// bufferSize:     bytes to buffer before flushing. 0 = unbuffered.
//                 Use 60*1024 for high-throughput scenarios.
func NewRegistry(writerLocation string, bufferSize uint) *Registry {
	loc := C.CString(writerLocation)
	defer C.free(unsafe.Pointer(loc))
	r := &Registry{
		ptr: C.spectator_registry_new(loc, C.uint(bufferSize)),
	}
	runtime.SetFinalizer(r, (*Registry).Close)
	return r
}

// Close releases the underlying C++ Registry. Safe to call multiple times.
func (r *Registry) Close() {
	if r.ptr != nil {
		C.spectator_registry_destroy(r.ptr)
		r.ptr = nil
	}
}

// toCArrays converts a Go map to the parallel C string arrays expected by the C API.
// The caller must free all returned pointers via freeCArrays.
func toCArrays(tags map[string]string) (**C.char, **C.char, C.int) {
	n := len(tags)
	if n == 0 {
		return nil, nil, 0
	}
	keys := make([]*C.char, n)
	vals := make([]*C.char, n)
	i := 0
	for k, v := range tags {
		keys[i] = C.CString(k)
		vals[i] = C.CString(v)
		i++
	}
	return (**C.char)(unsafe.Pointer(&keys[0])),
		(**C.char)(unsafe.Pointer(&vals[0])),
		C.int(n)
}

func freeCArrays(keys, vals **C.char, n C.int) {
	if n == 0 {
		return
	}
	ks := (*[1 << 20]*C.char)(unsafe.Pointer(keys))[:n:n]
	vs := (*[1 << 20]*C.char)(unsafe.Pointer(vals))[:n:n]
	for i := range ks {
		C.free(unsafe.Pointer(ks[i]))
		C.free(unsafe.Pointer(vs[i]))
	}
}

// -- Meter factory methods --

// Counter returns a Counter for the given name and tags.
func (r *Registry) Counter(name string, tags map[string]string) *Counter {
	return &Counter{r: r, name: name, tags: tags}
}

// Gauge returns a Gauge for the given name and tags.
func (r *Registry) Gauge(name string, tags map[string]string) *Gauge {
	return &Gauge{r: r, name: name, tags: tags, ttl: -1}
}

// GaugeWithTTL returns a Gauge that expires after ttlSeconds if not updated.
func (r *Registry) GaugeWithTTL(name string, tags map[string]string, ttlSeconds int) *Gauge {
	return &Gauge{r: r, name: name, tags: tags, ttl: ttlSeconds}
}

// MaxGauge returns a MaxGauge for the given name and tags.
func (r *Registry) MaxGauge(name string, tags map[string]string) *MaxGauge {
	return &MaxGauge{r: r, name: name, tags: tags}
}

// AgeGauge returns an AgeGauge for the given name and tags.
func (r *Registry) AgeGauge(name string, tags map[string]string) *AgeGauge {
	return &AgeGauge{r: r, name: name, tags: tags}
}

// MonotonicCounter returns a MonotonicCounter for the given name and tags.
func (r *Registry) MonotonicCounter(name string, tags map[string]string) *MonotonicCounter {
	return &MonotonicCounter{r: r, name: name, tags: tags}
}

// MonotonicCounterUint returns a MonotonicCounterUint for the given name and tags.
func (r *Registry) MonotonicCounterUint(name string, tags map[string]string) *MonotonicCounterUint {
	return &MonotonicCounterUint{r: r, name: name, tags: tags}
}

// Timer returns a Timer for the given name and tags.
func (r *Registry) Timer(name string, tags map[string]string) *Timer {
	return &Timer{r: r, name: name, tags: tags}
}

// PercentileTimer returns a PercentileTimer for the given name and tags.
func (r *Registry) PercentileTimer(name string, tags map[string]string) *PercentileTimer {
	return &PercentileTimer{r: r, name: name, tags: tags}
}

// DistributionSummary returns a DistributionSummary for the given name and tags.
func (r *Registry) DistributionSummary(name string, tags map[string]string) *DistributionSummary {
	return &DistributionSummary{r: r, name: name, tags: tags}
}

// PercentileDistributionSummary returns a PercentileDistributionSummary.
func (r *Registry) PercentileDistributionSummary(name string, tags map[string]string) *PercentileDistributionSummary {
	return &PercentileDistributionSummary{r: r, name: name, tags: tags}
}
