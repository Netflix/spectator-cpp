#pragma once

#include <libs/writer/writer_types/include/base/base_writer.h>
#include <string>
#include <vector>

class MemoryWriter final : public BaseWriter
{
   public:
    MemoryWriter() = default;
    ~MemoryWriter() override = default;

    void Write(const std::string& message) override;
    void Close() override;
    void Clear();

    const std::vector<std::string>& GetMessages() const noexcept { return m_messages; }

    const std::string& LastLine() const noexcept;

    bool IsEmpty() const noexcept { return m_messages.empty(); }

   private:
    std::vector<std::string> m_messages;
};
