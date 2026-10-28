#include <writer.h>

#include "writer_state.h"

#include <logger.h>

#include <stdexcept>

namespace spectator {

using detail::WriteResult;
using detail::WriterState;

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

// Trivially destructible, so it stays readable while other thread_local destructors run.
thread_local bool t_handleGone = false;

}  // namespace

// The calling thread's connection to the Writer's current state: the state itself and, when buffering, this thread's
// own buffer. One exists per thread, created on its first write and destroyed when the thread exits, which sends the
// thread's pending lines.
struct Writer::ThreadHandle
{
    // A write whose state is replaced mid-write reconnects and tries again, at most this many times.
    static constexpr int kMaxAttempts = 3;

    // Sends or buffers `line` for the calling thread. The line is dropped if the writer is not initialized.
    static void Write(Writer& writer, std::string_view line)
    {
        if (t_handleGone)
        {
            WriteWithoutHandle(writer, line);
            return;
        }
        Current().WriteWithReconnect(writer, line);
    }

    ~ThreadHandle()
    {
        if (buffer)
        {
            state->CloseBuffer(*buffer);
        }
        t_handleGone = true;
    }

    ThreadHandle(const ThreadHandle&) = delete;
    ThreadHandle& operator=(const ThreadHandle&) = delete;
    ThreadHandle(ThreadHandle&&) = delete;
    ThreadHandle& operator=(ThreadHandle&&) = delete;

   private:
    ThreadHandle() = default;

    static ThreadHandle& Current()
    {
        thread_local ThreadHandle handle;
        return handle;
    }

    void WriteWithReconnect(Writer& writer, std::string_view line)
    {
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
        {
            if (!state)
            {
                auto current = writer.SnapshotState();
                if (!current)
                {
                    writer.WarnNotInitialized();
                    return;
                }
                if (!Connect(std::move(current)))
                {
                    continue;
                }
            }
            if (Send(line) == WriteResult::Consumed)
            {
                return;
            }
            // The state was replaced or shut down after this thread connected. Closing it already sent this thread's
            // buffer, so just forget it.
            Disconnect();
        }
    }

    // For thread_local destructors that run after this thread's handle is gone: sends the line on its own, without
    // creating a new handle that nothing would flush.
    static void WriteWithoutHandle(Writer& writer, std::string_view line)
    {
        if (auto current = writer.SnapshotState())
        {
            std::string scratch;
            current->WriteLine(line, scratch);
        }
    }

    // Returns false if the state was closed before a buffer could be registered.
    bool Connect(std::shared_ptr<WriterState> next)
    {
        std::shared_ptr<WriterState::ThreadBuffer> nextBuffer;
        if (next->IsBuffered())
        {
            nextBuffer = next->RegisterBuffer();
            if (!nextBuffer)
            {
                return false;
            }
        }
        state = std::move(next);
        buffer = std::move(nextBuffer);
        return true;
    }

    void Disconnect() noexcept
    {
        buffer.reset();
        state.reset();
    }

    WriteResult Send(std::string_view line)
    {
        return buffer ? state->Append(*buffer, line) : state->WriteLine(line, scratch);
    }

    std::shared_ptr<WriterState> state;
    // Null when unbuffered.
    std::shared_ptr<WriterState::ThreadBuffer> buffer;
    // Reused by unbuffered sends.
    std::string scratch;
};

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
        GetInstance().Replace(std::make_shared<WriterState>(std::move(transport), EffectiveBufferConfig(type, buffer)));
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer)
{
    GetInstance().Replace(std::make_shared<WriterState>(std::move(transport), buffer));
}

void Writer::Shutdown() { GetInstance().Replace(nullptr); }

void Writer::ShutdownAtExit()
{
    Writer& writer = GetInstance();
    writer.m_exited.store(true, std::memory_order_relaxed);
    writer.Replace(nullptr);
}

void Writer::Replace(std::shared_ptr<WriterState> next)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (next && m_exited.load(std::memory_order_relaxed))
    {
        // The exit flush already ran, so nothing would send this state's buffers.
        next->Close();
        return;
    }
    // Stop and join the old drain thread and send its buffers before the new state takes over. Threads still
    // connected to the old state find it closed on their next write and reconnect to the current one.
    if (m_state)
    {
        m_state->Close();
    }
    m_state = std::move(next);
}

// ---- Writing ----

void Writer::Write(std::string_view line) { ThreadHandle::Write(GetInstance(), line); }

std::shared_ptr<WriterState> Writer::SnapshotState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
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
    auto state = GetInstance().SnapshotState();
    return state ? state->Transport() : nullptr;
}

void Writer::Flush()
{
    if (auto state = GetInstance().SnapshotState())
    {
        state->RunDrainPass();
    }
}

std::uint64_t Writer::DroppedCount()
{
    auto state = GetInstance().SnapshotState();
    return state ? state->DroppedCount() : 0;
}

std::size_t Writer::RegisteredBufferCount()
{
    auto state = GetInstance().SnapshotState();
    return state ? state->BufferCount() : 0;
}

}  // namespace spectator
