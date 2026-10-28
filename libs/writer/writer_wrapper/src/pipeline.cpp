#include <pipeline.h>

#include <logger.h>

#include <algorithm>

namespace spectator::detail {

std::shared_ptr<Pipeline> Pipeline::Create(std::unique_ptr<BaseWriter> transport, const BufferConfig& config)
{
    auto sink = std::make_shared<Sink>(std::move(transport));

    std::unique_ptr<ThreadRegistry> registry;
    if (config.size > 0)
    {
        // A full buffer is sent as one datagram, so it can't be larger than the transport allows.
        const std::size_t capacity = std::min<std::size_t>(config.size, sink->MaxPayloadSize());
        if (capacity < config.size)
        {
            Logger::warn("Writer: buffer size {} exceeds the largest datagram this transport can send; using {}",
                         config.size, capacity);
        }
        registry = std::make_unique<ThreadRegistry>(
            sink, RegistryOptions{.capacity = capacity, .dropOnFull = config.dropOnFull,
                                  .flushInterval = config.flushInterval});
        Logger::info("Writer buffering enabled: {} bytes per thread, flush interval {} ms, drop on full: {}", capacity,
                     config.flushInterval.count(), config.dropOnFull);
    }
    return std::shared_ptr<Pipeline>(new Pipeline(std::move(sink), std::move(registry)));
}

Pipeline::Pipeline(std::shared_ptr<Sink> sink, std::unique_ptr<ThreadRegistry> registry)
    : m_sink(std::move(sink)), m_registry(std::move(registry))
{
}

Pipeline::~Pipeline() { Close(); }

std::shared_ptr<ThreadBuffer> Pipeline::CreateThreadBuffer()
{
    return m_registry ? m_registry->CreateBuffer() : nullptr;
}

WriteResult Pipeline::SendLine(std::string_view line, std::string& scratch)
{
    scratch.assign(line);
    scratch.push_back('\n');
    return m_sink->Send(scratch);
}

void Pipeline::Close()
{
    if (m_registry)
    {
        m_registry->Close();
    }
    m_sink->Close();
}

void Pipeline::RunDrainPass()
{
    if (m_registry)
    {
        m_registry->RunDrainPass();
    }
}

std::uint64_t Pipeline::DroppedCount() const noexcept { return m_registry ? m_registry->DroppedCount() : 0; }

std::size_t Pipeline::ThreadBufferCount() const { return m_registry ? m_registry->BufferCount() : 0; }

}  // namespace spectator::detail
