#pragma once

#include <libs/utils/include/singleton.h>

#include <iostream>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>
#include <fmt/core.h>

constexpr const char *kMainLogger = "spectator";

class Logger final : public Singleton<Logger>
{
  private:
    spdlog::logger *m_logger; // Use raw pointer, not unique_ptr

    friend class Singleton<Logger>;

    Logger()
    {
        try
        {
            spdlog::init_thread_pool(8192, 1);
            auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto shared_logger =
                std::make_shared<spdlog::async_logger>(kMainLogger, sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
            shared_logger->set_level(spdlog::level::debug);
            spdlog::register_logger(shared_logger);
            m_logger = shared_logger.get();
        }
        catch (const spdlog::spdlog_ex &ex)
        {
            std::cerr << "Log initialization failed: " << ex.what() << "\n";
            m_logger = nullptr;
        }
    }

    ~Logger()                         = default;
    Logger(const Logger &)            = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&)                 = delete;
    Logger &operator=(Logger &&)      = delete;

  public:
    static spdlog::logger *GetLogger()
    {
        return GetInstance().m_logger;
    }

    static void debug(const std::string &msg)
    {
        GetLogger()->debug(msg);
    }

    static void info(const std::string &msg)
    {
        GetLogger()->info(msg);
    }

    static void warn(const std::string &msg)
    {
        GetLogger()->warn(msg);
    }

    static void error(const std::string &msg)
    {
        GetLogger()->error(msg);
    }

    template <typename... Args> static void debug(fmt::format_string<Args...> fmt, Args &&...args)
    {
        GetLogger()->debug(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args> static void info(fmt::format_string<Args...> fmt, Args &&...args)
    {
        GetLogger()->info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args> static void warn(fmt::format_string<Args...> fmt, Args &&...args)
    {
        GetLogger()->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args> static void error(fmt::format_string<Args...> fmt, Args &&...args)
    {
        GetLogger()->error(fmt, std::forward<Args>(args)...);
    }
};