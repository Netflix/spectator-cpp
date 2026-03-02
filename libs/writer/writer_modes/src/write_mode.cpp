#include "write_mode.h"
#include <memory_writer.h>
#include <udp_writer.h>
#include <uds_writer.h>
#include <logger.h>

#include <stdexcept>

namespace spectator {

std::unique_ptr<BaseWriter> WriteMode::CreateWriter(WriterType type, const std::string& param, int port)
{
    switch (type)
    {
        case WriterType::Memory:
            Logger::info("WriteMode initialized as MemoryWriter");
            return std::make_unique<MemoryWriter>();
        case WriterType::UDP:
            Logger::info("WriteMode initialized as UDPWriter with host: {} and port: {}", param, port);
            return std::make_unique<UDPWriter>(param, port);
        case WriterType::Unix:
            Logger::info("WriteMode initialized as UnixWriter with socket path: {}", param);
            return std::make_unique<UDSWriter>(param);
        default:
            throw std::runtime_error("Unsupported writer type");
    }
}

WriteMode::WriteMode(WriterType type, const std::string& param, int port)
    : m_writer(CreateWriter(type, param, port)) {}

}  // namespace spectator
