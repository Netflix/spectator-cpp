#include <writer.h>

#include <logger.h>
#include <pipeline.h>
#include <thread_session.h>

#include <stdexcept>

namespace spectator {

using detail::Pipeline;
using detail::ThreadSession;

namespace {

std::unique_ptr<BaseWriter> MakeTransport(WriterType type, const std::string& param, int port)
{
    switch (type)
    {
        case WriterType::Memory:
            Logger::info("WriterWrapper initialized as MemoryWriter");
            return std::make_unique<MemoryWriter>();
        case WriterType::UDP:
            Logger::info("WriterWrapper initialized as UDPWriter with host: {} and port: {}", param, port);
            return std::make_unique<UDPWriter>(param, port);
        case WriterType::Unix:
            Logger::info("WriterWrapper initialized as UnixWriter with socket path: {}", param);
            return std::make_unique<UDSWriter>(param);
        case WriterType::Noop:
            Logger::info("WriterWrapper initialized as NoopWriter");
            return std::make_unique<NoopWriter>();
    }
    throw std::runtime_error("Unsupported writer type");
}

// Only the socket writers buffer. Memory writes must be visible as soon as they happen, and the none writer has
// nothing to send.
BufferConfig EffectiveBufferConfig(WriterType type, BufferConfig config)
{
    const bool canBuffer = type == WriterType::UDP || type == WriterType::Unix;
    if (!canBuffer && config.size > 0)
    {
        Logger::info("Writer: the {} writer does not buffer; every line is sent as it is written",
                     WriterTypeToString(type));
        config.size = 0;
    }
    return config;
}

}  // namespace

// Sends every pending line during static destruction.
struct Writer::ExitGuard
{
    ExitGuard() = default;
    ExitGuard(const ExitGuard&) = delete;
    ExitGuard& operator=(const ExitGuard&) = delete;
    ~ExitGuard() { Writer::ShutdownAtExit(); }
};

// ---- Instance ----

Writer& Writer::GetInstance()
{
    static Writer* const instance = []
    {
        // Construct the logger first, so it is destroyed after ExitGuard, which may log.
        Logger::GetLogger();
        return new Writer();
    }();
    // Constructed after the logger, so it runs before the logger is destroyed.
    static ExitGuard exitGuard;
    return *instance;
}

// ---- Configuration ----

void Writer::Initialize(WriterType type, const std::string& param, int port, const BufferConfig& buffer)
{
    try
    {
        auto transport = MakeTransport(type, param, port);
        GetInstance().Replace(Pipeline::Create(std::move(transport), EffectiveBufferConfig(type, buffer)));
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer)
{
    GetInstance().Replace(Pipeline::Create(std::move(transport), buffer));
}

void Writer::Shutdown() { GetInstance().Replace(nullptr); }

void Writer::ShutdownAtExit()
{
    Writer& writer = GetInstance();
    writer.m_exited.store(true, std::memory_order_relaxed);
    writer.Replace(nullptr);
}

void Writer::Replace(std::shared_ptr<Pipeline> next)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (next && m_exited.load(std::memory_order_relaxed))
    {
        // The exit flush already ran, so nothing would send this pipeline's buffers.
        next->Close();
        return;
    }
    // Stop and join the old drain thread and send its buffers before the new pipeline takes over. Threads still
    // connected to the old pipeline find it closed on their next write and reconnect to the current one.
    if (m_pipeline)
    {
        m_pipeline->Close();
    }
    m_pipeline = std::move(next);
    m_hasPipeline.store(m_pipeline != nullptr, std::memory_order_release);
}

// ---- Writing ----

void Writer::Write(std::string_view line) { ThreadSession::Write(GetInstance(), line); }

std::shared_ptr<Pipeline> Writer::CurrentPipeline()
{
    if (!m_hasPipeline.load(std::memory_order_acquire))
    {
        WarnNotInitialized();
        return nullptr;
    }
    auto pipeline = SnapshotPipeline();
    if (!pipeline)
    {
        WarnNotInitialized();
    }
    return pipeline;
}

std::shared_ptr<Pipeline> Writer::SnapshotPipeline()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pipeline;
}

void Writer::WarnNotInitialized()
{
    if (m_exited.load(std::memory_order_relaxed) || m_warnedNotInitialized.exchange(true))
    {
        return;
    }
    Logger::warn("Writer is not initialized; metrics are dropped until a Registry is created");
}

// ---- Test hooks ----

BaseWriter* Writer::GetImpl()
{
    auto pipeline = GetInstance().SnapshotPipeline();
    return pipeline ? pipeline->Transport() : nullptr;
}

void Writer::Flush()
{
    if (auto pipeline = GetInstance().SnapshotPipeline())
    {
        pipeline->RunDrainPass();
    }
}

std::uint64_t Writer::DroppedCount()
{
    auto pipeline = GetInstance().SnapshotPipeline();
    return pipeline ? pipeline->DroppedCount() : 0;
}

std::size_t Writer::RegisteredBufferCount()
{
    auto pipeline = GetInstance().SnapshotPipeline();
    return pipeline ? pipeline->ThreadBufferCount() : 0;
}

}  // namespace spectator
