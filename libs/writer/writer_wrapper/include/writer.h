#pragma once

#include <buffer_config.h>
#include <pipeline_provider.h>
#include <writer_types.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace spectator {

namespace detail {
class Pipeline;
}  // namespace detail

// The process-wide writer every meter sends its protocol lines through.
//
// Unbuffered (buffer size 0, and always for the memory and none writers), each line goes straight to the shared
// sink. Buffered, each writing thread appends to its own buffer. A thread's buffer is sent when it fills (or, with
// drop-on-full, its lines are dropped and the drain thread is woken), when the drain thread runs every flush interval,
// and when the thread exits. A thread's lines always arrive in the order it wrote them.
class Writer final : private detail::PipelineProvider
{
   public:
    // The single instance. It is never destroyed, so threads still running during static destruction can call it
    // safely. An exit hook sends pending lines instead.
    static Writer& GetInstance();

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;

   private:
    friend class Registry;
    friend class WriterTestHelper;
    friend class AgeGauge;
    friend class Counter;
    friend class DistributionSummary;
    friend class Gauge;
    friend class MaxGauge;
    friend class MonotonicCounter;
    friend class MonotonicCounterUint;
    friend class PercentileDistributionSummary;
    friend class PercentileTimer;
    friend class Timer;

    struct ExitGuard;

    Writer() = default;
    ~Writer() = default;

    // ---- Used by Registry and the meters ----

    // Replaces the current configuration. The old one's drain thread is stopped and joined, its buffers are sent, and
    // its sink is closed before the new one takes over. If the new transport can't be built, the old configuration
    // stays in place and the exception propagates.
    static void Initialize(WriterType type, const std::string& param, int port, const BufferConfig& buffer);

    // Sends one protocol line. The writer adds the trailing newline.
    static void Write(std::string_view line);

    // Sends everything pending and closes the sink. Writes are dropped until the next Initialize.
    static void Shutdown();

    // ---- Test hooks, reached through WriterTestHelper ----

    static void InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer);
    static BaseWriter* GetImpl();
    static void Flush();
    static std::uint64_t DroppedCount();
    static std::size_t RegisteredBufferCount();

    // ---- Internals ----

    // Closes the current pipeline, then installs `next` (null to leave the writer uninitialized).
    void Replace(std::shared_ptr<detail::Pipeline> next);

    // Called at static destruction: shuts down and refuses further Initialize calls.
    static void ShutdownAtExit();

    // The current pipeline, or null (with a one-time warning) if the writer is not initialized. Thread sessions call
    // this to connect.
    std::shared_ptr<detail::Pipeline> CurrentPipeline() override;

    // The current pipeline, or null, without a warning.
    std::shared_ptr<detail::Pipeline> SnapshotPipeline();

    void WarnNotInitialized();

    // Guards m_pipeline, and serializes Replace so reconfiguring and shutting down never overlap.
    std::mutex m_mutex;
    std::shared_ptr<detail::Pipeline> m_pipeline;

    // Lets a write skip the mutex while no pipeline is installed.
    std::atomic<bool> m_hasPipeline{false};
    std::atomic<bool> m_exited{false};
    std::atomic<bool> m_warnedNotInitialized{false};
};

}  // namespace spectator
