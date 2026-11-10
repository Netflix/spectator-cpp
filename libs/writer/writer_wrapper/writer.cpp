#include <writer.h>

#include <buffered_writer.h>
#include <writer_types.h>
#include <logger.h>

#include <chrono>
#include <memory>
#include <stdexcept>

namespace spectator {

static constexpr auto NEW_LINE = '\n';

// How long a buffered send to a Unix socket waits for a receiver whose queue is full (see UDSWriter). Buffered
// sends come in bursts, and where the kernel does not make the sender wait, a burst that outruns the receiver
// would otherwise be dropped at the socket. Bounded, so that a receiver that has stopped reading costs the sender
// this much per buffer rather than stopping it for good.
static constexpr std::chrono::milliseconds RECEIVER_FULL_WAIT{250};

Writer::Writer()
{
    // The buffered writer logs from its sender thread and during shutdown, until the end of ~Writer. Constructing
    // the Logger before this constructor returns makes sure that it is destroyed after the Writer.
    Logger::GetLogger();
}

Writer::~Writer()
{
    // With buffering: stops the sender thread and sends what any thread still has buffered, including threads that
    // are still running. After it, nothing uses the socket writer any more, and lines that such threads record are
    // dropped.
    buffered_writer::ShutdownAtExit();
    this->Close();
}

void Writer::Initialize(WriterType type, const std::string& param, int port, unsigned int bufferSize,
                        std::chrono::milliseconds flushInterval, FullBufferPolicy fullBufferPolicy)
{
    // Get the singleton instance directly
    auto& instance = GetInstance();

    // Create the new writer based on type
    try
    {
        std::unique_ptr<BaseWriter> impl;
        switch (type)
        {
            case WriterType::Memory:
                impl = std::make_unique<MemoryWriter>();
                Logger::info("WriterWrapper initialized as MemoryWriter");
                break;
            case WriterType::UDP:
                impl = std::make_unique<UDPWriter>(param, port);
                Logger::info("WriterWrapper initialized as UDPWriter with host: {} and port: {}", param, port);
                break;
            case WriterType::Unix:
                // Only the buffered writer waits for a receiver whose queue is full. Without buffering the calling
                // thread sends, and a send fails at once as before.
                impl = std::make_unique<UDSWriter>(
                    param, bufferSize > 0 ? RECEIVER_FULL_WAIT : std::chrono::milliseconds::zero());
                Logger::info("WriterWrapper initialized as UnixWriter with socket path: {}", param);
                break;
            default:
                throw std::runtime_error("Unsupported writer type");
        }

        // An earlier initialization may have turned buffering on. Turn it off, which sends what is still buffered
        // through the old socket writer, before that one is replaced.
        buffered_writer::Stop();

        instance.m_impl = std::move(impl);
        instance.m_currentType = type;

        if (bufferSize > 0)
        {
            buffered_writer::Start(instance.m_impl.get(), bufferSize, flushInterval, fullBufferPolicy);
            Logger::info("WriterWrapper buffering enabled with size: {}, flush interval: {} ms, full buffer policy: {}",
                         bufferSize, flushInterval.count(), FullBufferPolicyToString(fullBufferPolicy));
        }
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to initialize writer: {}", e.what());
        throw;
    }
}

void Writer::TryToSend(const std::string& message)
{
    const auto& instance = GetInstance();
    instance.m_impl->Write(message);
}

void Writer::NonBufferedWrite(const std::string& message)
{
    // Since this is a non-static method, we're already operating on an instance
    // and can call the instance method directly
    this->TryToSend(message + NEW_LINE);
}

void Writer::Write(const std::string& line)
{
    // With buffering, the line goes into the calling thread's own buffer and the sender thread sends it.
    if (buffered_writer::Record(line))
    {
        return;
    }

    // Without buffering, the calling thread sends the line now.
    auto& instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Attempted to write with uninitialized writer implementation");
        return;
    }

    instance.NonBufferedWrite(line);
}

void Writer::Close()
{
    const auto& instance = GetInstance();

    if (!instance.m_impl)
    {
        Logger::error("Close called on uninitialized writer");
        return;
    }

    try
    {
        instance.m_impl->Close();
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

}  // namespace spectator
