#pragma once

#include <writer_types.h>

#include <string>
#include <stdexcept>

class WriterConfig
{
   public:
    explicit WriterConfig(const std::string& type);
    WriterConfig(const std::string& type, unsigned int bufferSize);

    [[nodiscard]] const WriterType& GetType() const noexcept { return m_type; }
    [[nodiscard]] unsigned int GetBufferSize() const noexcept { return m_bufferSize; }
    [[nodiscard]] bool IsBufferingEnabled() const noexcept { return m_isBufferingEnabled; }
    [[nodiscard]] const std::string& GetLocation() const noexcept { return m_location; }

   private:
    WriterType m_type;
    std::string m_location;
    unsigned int m_bufferSize = 0;
    bool m_isBufferingEnabled = false;
};