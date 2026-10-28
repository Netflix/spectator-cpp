#include <sink.h>

#include <logger.h>

#include <exception>

namespace spectator::detail {

Sink::Sink(std::unique_ptr<BaseWriter> transport)
    : m_transport(std::move(transport)), m_maxPayloadSize(m_transport->MaxPayloadSize())
{
}

Sink::~Sink() { Close(); }

WriteResult Sink::Send(std::string_view payload)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed)
    {
        return WriteResult::Closed;
    }
    try
    {
        m_transport->Write(payload);
    }
    catch (const std::exception& e)
    {
        Logger::error("Writer: failed to send {} bytes: {}", payload.size(), e.what());
    }
    return WriteResult::Consumed;
}

void Sink::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed)
    {
        return;
    }
    m_closed = true;
    try
    {
        m_transport->Close();
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to close writer: {}", e.what());
    }
}

}  // namespace spectator::detail
