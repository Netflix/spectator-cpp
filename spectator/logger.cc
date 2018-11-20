#include "logger.h"
#include <iostream>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace spectator {

LogManager& log_manager() noexcept {
  static auto* the_log_manager = new LogManager();
  return *the_log_manager;
}

LogManager::LogManager() noexcept {
  try {
    // use a queue with a max of 8k entries and one thread
    spdlog::init_thread_pool(8192, 1);
    current_logger_ =
        spdlog::create_async_nb<spdlog::sinks::ansicolor_stdout_sink_mt>(name_);
    current_logger_->set_level(spdlog::level::debug);
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Log initialization failed: " << ex.what() << "\n";
  }
}

std::shared_ptr<spdlog::logger> LogManager::Logger() noexcept {
  return current_logger_;
}

}  // namespace spectator
