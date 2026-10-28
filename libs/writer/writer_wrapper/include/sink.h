#pragma once

#include <base_writer.h>
#include <write_result.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>

namespace spectator::detail {

// The one transport every thread sends through. A mutex serializes sends, so the writing threads and the drain thread
// share a single socket.
class Sink
{
   public:
    explicit Sink(std::unique_ptr<BaseWriter> transport);
    ~Sink();

    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    Sink(Sink&&) = delete;
    Sink& operator=(Sink&&) = delete;

    // Sends the payload with one transport Write. Returns Closed, without sending, once the sink is closed.
    WriteResult Send(std::string_view payload);

    // Closes the transport; later sends return false. Safe to call more than once.
    void Close();

    // The largest payload one Send can deliver, read from the transport when the sink was created.
    [[nodiscard]] std::size_t MaxPayloadSize() const noexcept { return m_maxPayloadSize; }

    // The transport, for tests that inspect a MemoryWriter.
    [[nodiscard]] BaseWriter* Transport() const noexcept { return m_transport.get(); }

   private:
    std::mutex m_mutex;
    const std::unique_ptr<BaseWriter> m_transport;
    const std::size_t m_maxPayloadSize;
    bool m_closed = false;
};

}  // namespace spectator::detail
