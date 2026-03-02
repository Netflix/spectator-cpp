#pragma once

#include <base_writer.h>

#include <memory>
#include <string>

namespace spectator {

enum class WriteModeType
{
    NonBuffered,
    Buffered,
    ThreadLocalBuffered
};

class WriteMode
{
   public:
    explicit WriteMode(std::unique_ptr<BaseWriter> writer) : m_writer(std::move(writer)) {}
    virtual ~WriteMode() { if (m_writer) m_writer->Close(); }

    WriteMode(const WriteMode&) = delete;
    WriteMode& operator=(const WriteMode&) = delete;
    WriteMode(WriteMode&&) = delete;
    WriteMode& operator=(WriteMode&&) = delete;

    virtual WriteModeType GetType() const = 0;
    virtual void Write(const std::string& message) = 0;

    BaseWriter* GetWriter() const { return m_writer.get(); }

   protected:
    std::unique_ptr<BaseWriter> m_writer;
};

}  // namespace spectator
