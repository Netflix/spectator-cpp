package spectator

/*
#include "spectator_c.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"strings"
	"time"
	"unsafe"
)

func Example() {
	// Use the memory writer so we can inspect the output at the end.
	// For production, replace "memory" with "udp" or "uds".
	r := NewRegistry("memory", 0)
	defer r.Close()

	// --- Counter ---
	requests := r.Counter("server.requests", map[string]string{
		"status":   "200",
		"method":   "GET",
		"endpoint": "/api/v1/data",
	})
	requests.Increment()
	requests.Add(5)

	// --- Gauge ---
	queueDepth := r.Gauge("queue.depth", map[string]string{"queue": "ingest"})
	queueDepth.Set(42.0)

	// Gauge with TTL — expires if not updated within 30 seconds.
	activeSessions := r.GaugeWithTTL("active.sessions", nil, 30)
	activeSessions.Set(128.0)

	// --- MaxGauge ---
	peakMemory := r.MaxGauge("memory.peak.bytes", map[string]string{"region": "heap"})
	peakMemory.Set(512 * 1024 * 1024)

	// --- AgeGauge ---
	lastSync := r.AgeGauge("data.last.sync.age", map[string]string{"source": "primary"})
	lastSync.Now()

	// --- MonotonicCounter ---
	bytesRead := r.MonotonicCounter("bytes.read", map[string]string{"device": "eth0"})
	bytesRead.Set(1_000_000)

	bytesWritten := r.MonotonicCounterUint("bytes.written", map[string]string{"device": "eth0"})
	bytesWritten.Set(500_000)

	// --- Timer ---
	dbQuery := r.Timer("db.query.latency", map[string]string{"table": "users"})
	start := time.Now()
	time.Sleep(2 * time.Millisecond)
	dbQuery.Record(time.Since(start).Seconds())

	// --- PercentileTimer ---
	httpLatency := r.PercentileTimer("http.request.latency", map[string]string{
		"method": "POST",
		"path":   "/api/v1/ingest",
	})
	httpLatency.Record(0.042)

	// --- DistributionSummary ---
	payloadSize := r.DistributionSummary("http.request.size", map[string]string{
		"endpoint": "/api/v1/ingest",
	})
	payloadSize.Record(4096)

	// --- PercentileDistributionSummary ---
	responseSize := r.PercentileDistributionSummary("http.response.size", map[string]string{
		"endpoint": "/api/v1/data",
	})
	responseSize.Record(1024)

	// --- Print all recorded lines from the memory writer ---
	raw := C.spectator_memory_writer_dump(r.ptr)
	defer C.spectator_free_string(raw)

	lines := strings.Split(strings.TrimRight(C.GoString(raw), "\n"), "\n")
	fmt.Printf("Recorded %d metric lines:\n", len(lines))
	for _, line := range lines {
		fmt.Println(" ", line)
	}
}

// DumpLines returns all lines written to the memory writer as a slice.
// Only meaningful when the registry was created with location "memory".
func (r *Registry) DumpLines() []string {
	raw := C.spectator_memory_writer_dump(r.ptr)
	defer C.spectator_free_string(raw)
	s := C.GoString(raw)
	if s == "" {
		return nil
	}
	lines := strings.Split(strings.TrimRight(s, "\n"), "\n")
	result := make([]string, 0, len(lines))
	for _, l := range lines {
		if l != "" {
			result = append(result, l)
		}
	}
	return result
}

// ensure unsafe is used (CGo requires it for pointer conversions)
var _ = unsafe.Pointer(nil)
