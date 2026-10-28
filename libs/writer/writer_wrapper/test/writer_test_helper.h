#pragma once

#include <writer.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace spectator {

/**
 * WriterTestHelper - reaches the Writer's private API for tests. It is a friend of Writer.
 *
 * Two kinds of access, kept apart from the production lifecycle:
 * - The process-wide writer, which the meters and the Registry use. The writer is configured once per process, so
 *   tests reset it between tests.
 * - Standalone writers, which unit tests create to exercise buffering without going through the singleton.
 */
class WriterTestHelper
{
   public:
    // ---- The process-wide writer ----

    // Sends everything pending and replaces the writer with a fresh, unconfigured one. The Initialize functions below
    // do it first.
    static void Reset() { Writer::ResetForTesting(); }

    static void InitializeWriter(WriterType type, const std::string& param = "", int port = 0,
                                 unsigned int bufferSize = 0)
    {
        Reset();
        Writer::Initialize(type, param, port, BufferConfig{.size = bufferSize});
    }

    static void InitializeWriter(WriterType type, const std::string& param, int port, const BufferConfig& buffer)
    {
        Reset();
        Writer::Initialize(type, param, port, buffer);
    }

    // Installs any transport, buffered whenever buffer.size > 0, including a MemoryWriter or a test transport.
    static void InitializeWithTransport(std::unique_ptr<BaseWriter> transport, const BufferConfig& buffer = {})
    {
        Reset();
        Writer::InitializeWithTransport(std::move(transport), buffer);
    }

    static void Shutdown() { Reset(); }

    // The transport of the current configuration, or null if the writer is not configured.
    static BaseWriter* GetImpl() { return Writer::GetImpl(); }

    // Runs one drain pass now, on the calling thread.
    static void Flush() { Writer::Flush(); }

    static std::uint64_t DroppedCount() { return Writer::DroppedCount(); }

    static std::size_t RegisteredBufferCount() { return Writer::RegisteredBufferCount(); }

    // ---- Standalone writers ----

    static std::unique_ptr<Writer> MakeWriter(std::unique_ptr<BaseWriter> transport, const BufferConfig& config)
    {
        std::unique_ptr<Writer> writer(new Writer());
        writer->Configure(std::move(transport), config);
        return writer;
    }

    static bool IsBuffered(const Writer& writer) { return writer.IsBuffered(); }

    static std::shared_ptr<detail::ThreadBuffer> RegisterBuffer(Writer& writer) { return writer.RegisterBuffer(); }

    static void Append(Writer& writer, detail::ThreadBuffer& buffer, std::string_view line)
    {
        writer.Append(buffer, line);
    }

    static void RetireBuffer(Writer& writer, detail::ThreadBuffer& buffer) { writer.RetireBuffer(buffer); }

    static void WriteLine(Writer& writer, std::string_view line, std::string& scratch)
    {
        writer.WriteLine(line, scratch);
    }

    static void RunDrainPass(Writer& writer) { writer.RunDrainPass(); }

    static void Shutdown(Writer& writer) { writer.Shutdown(); }

    static std::uint64_t DroppedCount(const Writer& writer) { return writer.DroppedLines(); }

    static std::size_t BufferCount(const Writer& writer) { return writer.BufferCount(); }
};

}  // namespace spectator
