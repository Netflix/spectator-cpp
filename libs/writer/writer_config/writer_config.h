#pragma once

#include <writer_types.h>
#include <write_mode.h>

#include <string>
#include <stdexcept>

namespace spectator {

class WriterConfig
{
   public:
    explicit WriterConfig(const std::string& type);
    WriterConfig(const std::string& type, unsigned int bufferSize,
                 WriteModeType modeType = WriteModeType::Auto);

    [[nodiscard]] const WriterType& GetType() const noexcept { return m_type; }
    [[nodiscard]] unsigned int GetBufferSize() const noexcept { return m_bufferSize; }
    [[nodiscard]] const std::string& GetLocation() const noexcept { return m_location; }
    [[nodiscard]] WriteModeType GetModeType() const noexcept { return m_modeType; }

   private:
    WriterType m_type;
    std::string m_location;
    unsigned int m_bufferSize = 0;
    WriteModeType m_modeType = WriteModeType::Auto;
};

}  // namespace spectator