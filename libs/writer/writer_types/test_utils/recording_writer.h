#pragma once

#include <base_writer.h>

#include <condition_variable>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace spectator::test {

// Everything a set of RecordingWriters received. It is shared, so it outlives the transports: the Writer destroys a
// transport when it is reconfigured.
class Recording
{
   public:
    struct Datagram
    {
        std::string payload;
        std::thread::id sender;
        int transport = 0;
    };

    void Add(Datagram datagram)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        ++m_blockedWriters;
        m_unblocked.wait(lock, [this] { return !m_blocked; });
        --m_blockedWriters;
        m_datagrams.push_back(std::move(datagram));
    }

    // While blocked, every Write waits, like a send to a receiver that has stopped reading.
    void Block()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_blocked = true;
    }

    void Unblock()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_blocked = false;
        }
        m_unblocked.notify_all();
    }

    // Writes currently waiting while blocked.
    [[nodiscard]] int BlockedWriters() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_blocked ? m_blockedWriters : 0;
    }

    [[nodiscard]] std::vector<Datagram> Datagrams() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_datagrams;
    }

    [[nodiscard]] std::size_t Count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_datagrams.size();
    }

    [[nodiscard]] std::vector<std::string> Payloads() const
    {
        std::vector<std::string> payloads;
        for (const auto& datagram : Datagrams())
        {
            payloads.push_back(datagram.payload);
        }
        return payloads;
    }

    // Every line received, in arrival order, without its newline.
    [[nodiscard]] std::vector<std::string> Lines() const
    {
        std::vector<std::string> lines;
        for (const auto& datagram : Datagrams())
        {
            std::istringstream stream(datagram.payload);
            for (std::string line; std::getline(stream, line);)
            {
                lines.push_back(line);
            }
        }
        return lines;
    }

   private:
    mutable std::mutex m_mutex;
    std::condition_variable m_unblocked;
    std::vector<Datagram> m_datagrams;
    bool m_blocked = false;
    int m_blockedWriters = 0;
};

// A transport that records each Write, with the thread that made it, into a shared Recording.
class RecordingWriter final : public BaseWriter
{
   public:
    explicit RecordingWriter(std::shared_ptr<Recording> recording, int id = 0,
                             std::size_t maxPayloadSize = std::numeric_limits<std::size_t>::max())
        : m_recording(std::move(recording)), m_id(id), m_maxPayloadSize(maxPayloadSize)
    {
    }

    void Write(std::string_view payload) override
    {
        m_recording->Add({std::string(payload), std::this_thread::get_id(), m_id});
    }

    void Close() override {}

    [[nodiscard]] std::size_t MaxPayloadSize() const noexcept override { return m_maxPayloadSize; }

   private:
    const std::shared_ptr<Recording> m_recording;
    const int m_id;
    const std::size_t m_maxPayloadSize;
};

}  // namespace spectator::test
