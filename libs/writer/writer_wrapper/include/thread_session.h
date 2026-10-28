#pragma once

#include <pipeline.h>
#include <pipeline_provider.h>
#include <thread_buffer.h>
#include <write_result.h>

#include <memory>
#include <string>
#include <string_view>

namespace spectator::detail {

// The calling thread's connection to the Writer's current Pipeline: the pipeline itself and, when buffering, this
// thread's own buffer. One exists per thread, created on its first write and destroyed when the thread exits, which
// sends the thread's pending lines.
//
// Write is the only entry point. It connects on first use, reconnects after the pipeline is replaced or shut down, and
// covers writes that arrive after the thread's session has been destroyed.
class ThreadSession
{
   public:
    // Sends or buffers `line` for the calling thread. Never throws. The line is dropped if the writer is not
    // initialized.
    static void Write(PipelineProvider& provider, std::string_view line);

    ~ThreadSession();

    ThreadSession(const ThreadSession&) = delete;
    ThreadSession& operator=(const ThreadSession&) = delete;
    ThreadSession(ThreadSession&&) = delete;
    ThreadSession& operator=(ThreadSession&&) = delete;

   private:
    ThreadSession() = default;

    static ThreadSession& Current();

    // A write whose pipeline is replaced mid-write reconnects and tries again, at most this many times.
    static constexpr int kMaxAttempts = 3;

    void WriteConnected(PipelineProvider& provider, std::string_view line);

    // For thread_local destructors that run after this thread's session is gone: sends the line on its own, without
    // creating a new session that nothing would flush.
    static void WriteWithoutSession(PipelineProvider& provider, std::string_view line);

    [[nodiscard]] bool IsConnected() const noexcept { return m_pipeline != nullptr; }

    // Returns false if the pipeline was closed before a buffer could be registered.
    bool Connect(std::shared_ptr<Pipeline> pipeline);

    // Forgets the pipeline. Used after it was replaced or shut down, which already sent this thread's buffer.
    void Disconnect() noexcept;

    // Closed only if the pipeline was closed. A line that was buffered, sent or dropped is Consumed.
    WriteResult Send(std::string_view line);

    std::shared_ptr<Pipeline> m_pipeline;
    // Null when unbuffered.
    std::shared_ptr<ThreadBuffer> m_buffer;
    // Reused by unbuffered sends.
    std::string m_scratch;
};

}  // namespace spectator::detail
