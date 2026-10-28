#pragma once

#include <base_writer.h>
#include <buffer_config.h>
#include <sink.h>
#include <thread_buffer.h>
#include <thread_registry.h>
#include <write_result.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace spectator::detail {

// One complete writer configuration: the sink that owns the transport, plus, when buffering is on, the registry of
// per-thread buffers and its drain thread. The Writer replaces its Pipeline as a whole when it is reconfigured, so
// everything that belongs together is created, used and closed together.
class Pipeline
{
   public:
    // Builds the sink and, if config.size > 0, the registry and its drain thread. The buffer size is clamped to the
    // transport's largest datagram. Throws if the drain thread can't start.
    static std::shared_ptr<Pipeline> Create(std::unique_ptr<BaseWriter> transport, const BufferConfig& config);

    // Closes the pipeline.
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    [[nodiscard]] bool IsBuffered() const noexcept { return m_registry != nullptr; }

    // A new buffer for the calling thread. Null if the pipeline is unbuffered or already closed.
    std::shared_ptr<ThreadBuffer> CreateThreadBuffer();

    // Unbuffered path: sends `line` and a newline as one datagram. `scratch` holds the combined payload so repeated
    // calls don't allocate. Returns Closed, without sending, if the pipeline is closed.
    WriteResult SendLine(std::string_view line, std::string& scratch);

    // Stops the drain thread, sends every thread's pending lines, then closes the sink. Safe to call more than once.
    void Close();

    // Test and diagnostic hooks.
    void RunDrainPass();
    [[nodiscard]] BaseWriter* Transport() const noexcept { return m_sink->Transport(); }
    [[nodiscard]] std::uint64_t DroppedCount() const noexcept;
    [[nodiscard]] std::size_t ThreadBufferCount() const;

   private:
    Pipeline(std::shared_ptr<Sink> sink, std::unique_ptr<ThreadRegistry> registry);

    const std::shared_ptr<Sink> m_sink;
    // Null when unbuffered.
    const std::unique_ptr<ThreadRegistry> m_registry;
};

}  // namespace spectator::detail
