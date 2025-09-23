#pragma once

#include <singleton.h>

#include <memory>
#include <string>
#include <format>

class Logger final : public Singleton<Logger>
{
   private:
    // Forward declaration for pimpl
    class LoggerImpl;
    std::unique_ptr<LoggerImpl> m_impl;

    friend class Singleton<Logger>;

    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

   public:
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);

    template <typename... Args>
    static void debug(std::format_string<Args...> fmt, Args&&... args)
    {
        debug(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args)
    {
        info(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args)
    {
        warn(std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args)
    {
        error(std::format(fmt, std::forward<Args>(args)...));
    }
};