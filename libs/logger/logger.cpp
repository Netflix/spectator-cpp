#include "logger.h"

#include <iostream>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>

constexpr const char* kMainLogger = "spectator";

class Logger::LoggerImpl
{
   public:
    std::shared_ptr<spdlog::logger> m_logger;

    LoggerImpl()
    {
        try
        {
            m_logger = spdlog::create_async_nb<spdlog::sinks::ansicolor_stdout_sink_mt>(kMainLogger);
            if (m_logger == nullptr)
            {
                throw std::runtime_error("Failed to create logger: spdlog returned null");
            }
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            throw std::runtime_error("Log initialization failed: " + std::string(ex.what()));
        }
    }

    ~LoggerImpl() = default;

    spdlog::logger* GetLogger() const 
    { 
        return m_logger.get(); 
    }
};

Logger::Logger() : m_impl(std::make_unique<LoggerImpl>())
{
}

Logger::~Logger() = default;

void Logger::debug(const std::string& msg)
{
    auto* logger = GetInstance().m_impl->GetLogger();
    logger->debug(msg);
}

void Logger::info(const std::string& msg)
{
    auto* logger = GetInstance().m_impl->GetLogger();
    logger->info(msg);
}

void Logger::warn(const std::string& msg)
{
    auto* logger = GetInstance().m_impl->GetLogger();
    logger->warn(msg);
}

void Logger::error(const std::string& msg)
{
    auto* logger = GetInstance().m_impl->GetLogger();
    logger->error(msg);
}
