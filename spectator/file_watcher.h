#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <unistd.h>

namespace atlas {
namespace util {

class FileWatcher {
 public:
  explicit FileWatcher(std::string file_name) noexcept
      : file_name_(std::move(file_name)),
        last_updated_(now()),
        exists_(access(file_name_.c_str(), F_OK) != -1) {}

  FileWatcher(const FileWatcher& other) noexcept
      : file_name_(other.file_name_),
        last_updated_(other.last_updated_.load(std::memory_order_relaxed)),
        exists_(other.exists_.load(std::memory_order_relaxed)) {}

  FileWatcher& operator=(const FileWatcher& other) noexcept {
    file_name_ = other.file_name_;
    exists_.store(other.exists_, std::memory_order_relaxed);
    last_updated_.store(other.last_updated_, std::memory_order_relaxed);
    return *this;
  }

  FileWatcher(FileWatcher&& other) noexcept
      : file_name_(std::move(other.file_name_)),
        last_updated_(other.last_updated_.load(std::memory_order_relaxed)),
        exists_(other.exists_.load(std::memory_order_relaxed)) {}

  FileWatcher& operator=(FileWatcher&& other) noexcept {
    file_name_ = std::move(other.file_name_);
    exists_.store(other.exists_, std::memory_order_relaxed);
    last_updated_.store(other.last_updated_, std::memory_order_relaxed);
    return *this;
  }

  bool exists() const noexcept {
    auto last = last_updated_.load(std::memory_order_relaxed);
    auto ts = now();
    if ((ts - last) > kMaxStaleMillis) {
      exists_.store(access(file_name_.c_str(), F_OK) != -1,
                    std::memory_order_relaxed);
      last_updated_.store(ts, std::memory_order_relaxed);
    }
    return exists_.load(std::memory_order_relaxed);
  }

  std::string file_name() const noexcept { return file_name_; }

 private:
  static constexpr int kMaxStaleMillis = 2000;
  static int64_t now() {
    return std::chrono::system_clock::now().time_since_epoch().count();
  }

  std::string file_name_;
  mutable std::atomic<int64_t> last_updated_;
  mutable std::atomic<bool> exists_;
};

}  // namespace util
}  // namespace atlas
