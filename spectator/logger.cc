#include "logger.h"
#include <iostream>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace spectator {

static constexpr const char* const kMainLogger = "spectator";

LogManager& log_manager() noexcept {
  static auto* the_log_manager = new LogManager();
  return *the_log_manager;
}

LogManager::LogManager() noexcept {
  try {
    logger_ = spdlog::create_async_nb<spdlog::sinks::ansicolor_stdout_sink_mt>(
        kMainLogger);
    logger_->set_level(spdlog::level::debug);
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Log initialization failed: " << ex.what() << "\n";
  }
}

std::shared_ptr<spdlog::logger> LogManager::Logger() noexcept {
  return logger_;
}

}  // namespace spectator
