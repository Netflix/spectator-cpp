#pragma once

#include <base_writer.h>

#include <mutex>
#include <string>
#include <vector>

namespace spectator {

// Keeps every payload in memory, one entry per Write. Used by tests. It is safe to read from one thread while others
// write; the accessors return copies for that reason.
class MemoryWriter final : public BaseWriter
{
   public:
    MemoryWriter() = default;
    ~MemoryWriter() override = default;

    void Write(std::string_view message) override;
    void Close() override;
    void Clear();

    [[nodiscard]] std::vector<std::string> GetMessages() const;

    // The most recent payload, or an empty string when there is none.
    [[nodiscard]] std::string LastLine() const;

    [[nodiscard]] bool IsEmpty() const;

   private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_messages;
};

}  // namespace spectator
