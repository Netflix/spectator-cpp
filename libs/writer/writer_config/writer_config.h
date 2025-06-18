#pragma once

#include <libs/writer/writer_types/include/writer_types.h>

#include <string>
#include <stdexcept>

class WriterConfig
{
   public:
    WriterConfig(const std::string& type);
    WriterConfig(const std::string& type, unsigned int bufferSize);

    const WriterType& GetType() const noexcept { return m_type; }
    unsigned int GetBufferSize() const noexcept { return m_bufferSize; }
    bool IsBufferingEnabled() const noexcept { return m_isBufferingEnabled; }
    const std::string& GetLocation() const noexcept { return m_location; }

   private:
    WriterType m_type;
    std::string m_location;
    unsigned int m_bufferSize;
    bool m_isBufferingEnabled = false;
};