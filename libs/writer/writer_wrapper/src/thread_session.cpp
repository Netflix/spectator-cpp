#include <thread_session.h>

namespace spectator::detail {

namespace {
// Trivially destructible, so it stays readable while other thread_local destructors run.
thread_local bool t_sessionGone = false;
}  // namespace

void ThreadSession::Write(PipelineProvider& provider, std::string_view line)
{
    if (t_sessionGone)
    {
        WriteWithoutSession(provider, line);
        return;
    }
    Current().WriteConnected(provider, line);
}

ThreadSession& ThreadSession::Current()
{
    thread_local ThreadSession session;
    return session;
}

ThreadSession::~ThreadSession()
{
    if (m_buffer)
    {
        m_buffer->FlushAndClose();
    }
    t_sessionGone = true;
}

void ThreadSession::WriteConnected(PipelineProvider& provider, std::string_view line)
{
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
    {
        if (!IsConnected())
        {
            auto pipeline = provider.CurrentPipeline();
            if (!pipeline)
            {
                return;
            }
            if (!Connect(std::move(pipeline)))
            {
                continue;
            }
        }
        if (Send(line) == WriteResult::Consumed)
        {
            return;
        }
        // The pipeline was replaced or shut down after this thread connected.
        Disconnect();
    }
}

void ThreadSession::WriteWithoutSession(PipelineProvider& provider, std::string_view line)
{
    if (auto pipeline = provider.CurrentPipeline())
    {
        std::string scratch;
        pipeline->SendLine(line, scratch);
    }
}

bool ThreadSession::Connect(std::shared_ptr<Pipeline> pipeline)
{
    std::shared_ptr<ThreadBuffer> buffer;
    if (pipeline->IsBuffered())
    {
        buffer = pipeline->CreateThreadBuffer();
        if (!buffer)
        {
            return false;
        }
    }
    m_pipeline = std::move(pipeline);
    m_buffer = std::move(buffer);
    return true;
}

void ThreadSession::Disconnect() noexcept
{
    m_buffer.reset();
    m_pipeline.reset();
}

WriteResult ThreadSession::Send(std::string_view line)
{
    if (m_buffer)
    {
        // Appended and Dropped both mean the buffer took the decision; only Closed needs a reconnect.
        const auto result = m_buffer->Append(line);
        return result == ThreadBuffer::AppendResult::Closed ? WriteResult::Closed : WriteResult::Consumed;
    }
    return m_pipeline->SendLine(line, m_scratch);
}

}  // namespace spectator::detail
