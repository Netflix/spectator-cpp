#pragma once

#include <writer_types.h>

#include <chrono>
#include <cstddef>
#include <string>

namespace spectator {

// The buffered mode of the Writer. Each thread that records appends its lines to its own buffer, and one sender
// thread sends the buffers through the socket writer. Internal to the writer wrapper: only Writer uses it. The
// design is described in buffered_writer.cpp.
namespace buffered_writer {

// Turns buffering on and starts the sender thread. Lines are sent through `socketWriter`, which has to stay valid
// until Stop() or ShutdownAtExit() has returned. Not thread-safe: Writer::Initialize calls it while no other thread
// is recording, and only while buffering is off.
void Start(BaseWriter* socketWriter, std::size_t bufferSize, std::chrono::milliseconds flushInterval,
           FullBufferPolicy fullBufferPolicy);

// Turns buffering off again, if it is on: stops the sender thread and sends what is still buffered. Used before the
// writer is initialized again. Not thread-safe, like Start().
void Stop();

// For process exit. Like Stop(), it stops the sender thread and sends what is still buffered, but buffering stays
// on: threads that are still recording keep being handled here, where their lines are now dropped, and are never
// sent on to a Writer that is being destroyed. Does nothing if buffering is off.
void ShutdownAtExit();

// Records a line, given without its trailing newline, if buffering is on. Returns false if buffering is off, and
// the caller then has to send the line itself. Thread-safe.
bool Record(const std::string& line);

}  // namespace buffered_writer

}  // namespace spectator
