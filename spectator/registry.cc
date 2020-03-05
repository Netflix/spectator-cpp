#include "logger.h"
#include "registry.h"
#include <fmt/ostream.h>

namespace spectator {

Registry::Registry(std::unique_ptr<Config> config,
                   Registry::logger_ptr logger) noexcept
    : should_stop_{true},
      meter_ttl_{config->meter_ttl},
      config_{std::move(config)},
      logger_{std::move(logger)},
      publisher_(this) {}

const Config& Registry::GetConfig() const noexcept { return *config_; }

Registry::logger_ptr Registry::GetLogger() const noexcept { return logger_; }

IdPtr Registry::CreateId(std::string name, Tags tags) const noexcept {
  return std::make_shared<Id>(name, tags);
}

std::shared_ptr<Counter> Registry::GetCounter(IdPtr id) noexcept {
  return create_and_register_as_needed<Counter>(std::move(id));
}

std::shared_ptr<Counter> Registry::GetCounter(std::string name) noexcept {
  return GetCounter(CreateId(std::move(name), Tags{}));
}

std::shared_ptr<DistributionSummary> Registry::GetDistributionSummary(
    IdPtr id) noexcept {
  return create_and_register_as_needed<DistributionSummary>(std::move(id));
}

std::shared_ptr<DistributionSummary> Registry::GetDistributionSummary(
    std::string name) noexcept {
  return GetDistributionSummary(CreateId(std::move(name), Tags{}));
}

std::shared_ptr<Gauge> Registry::GetGauge(IdPtr id) noexcept {
  return create_and_register_as_needed<Gauge>(std::move(id));
}

std::shared_ptr<Gauge> Registry::GetGauge(std::string name) noexcept {
  return GetGauge(CreateId(std::move(name), Tags{}));
}

std::shared_ptr<MaxGauge> Registry::GetMaxGauge(IdPtr id) noexcept {
  return create_and_register_as_needed<MaxGauge>(std::move(id));
}

std::shared_ptr<MaxGauge> Registry::GetMaxGauge(std::string name) noexcept {
  return GetMaxGauge(CreateId(std::move(name), Tags{}));
}

std::shared_ptr<MonotonicCounter> Registry::GetMonotonicCounter(
    IdPtr id) noexcept {
  return create_and_register_as_needed<MonotonicCounter>(std::move(id));
}

std::shared_ptr<MonotonicCounter> Registry::GetMonotonicCounter(
    std::string name) noexcept {
  return GetMonotonicCounter(CreateId(std::move(name), Tags{}));
}

std::shared_ptr<Timer> Registry::GetTimer(IdPtr id) noexcept {
  return create_and_register_as_needed<Timer>(std::move(id));
}

std::shared_ptr<Timer> Registry::GetTimer(std::string name) noexcept {
  return GetTimer(CreateId(std::move(name), Tags{}));
}

// only insert if it doesn't exist, otherwise return the existing meter
std::shared_ptr<Meter> Registry::insert_if_needed(
    std::shared_ptr<Meter> meter) noexcept {
  std::lock_guard<std::mutex> lock(meters_mutex);
  auto insert_result = meters_.emplace(meter->MeterId(), meter);
  auto ret = insert_result.first->second;
  return ret;
}

void Registry::log_type_error(const Id& id, MeterType prev_type,
                              MeterType attempted_type) const noexcept {
  logger_->error(
      "Attempted to register meter {} as type {} but previously registered as "
      "{}",
      id, attempted_type, prev_type);
}

void Registry::Start() noexcept {
  publisher_.Start();
  if (!should_stop_.exchange(false)) {
    // already started
    return;
  }

  expirer_thread_ = std::thread(&Registry::expirer, this);
}

void Registry::Stop() noexcept {
  publisher_.Stop();
  if (!should_stop_.exchange(true)) {
    logger_->debug("Stopping expirer thread");
    cv_.notify_all();
    expirer_thread_.join();
  }
}

inline bool is_meter_expired(std::chrono::system_clock::time_point now,
                             const Meter& m,
                             std::chrono::milliseconds meter_ttl) {
  if (meter_ttl.count() == 0) {
    return false;
  }
  auto updated_ago = now - m.Updated();
  return updated_ago > meter_ttl;
}

std::vector<Measurement> Registry::Measurements() const noexcept {
  auto now = std::chrono::system_clock::now();
  std::vector<Measurement> res;
  std::lock_guard<std::mutex> lock{meters_mutex};
  for (const auto& pair : meters_) {
    const auto& m = *pair.second;
    if (!is_meter_expired(now, m, meter_ttl_)) {
      auto ms = m.Measure();
      std::move(ms.begin(), ms.end(), std::back_inserter(res));
    }
  }
  return res;
}

std::vector<std::shared_ptr<Meter>> Registry::Meters() const noexcept {
  std::vector<std::shared_ptr<Meter>> res;
  std::lock_guard<std::mutex> lock{meters_mutex};
  for (const auto& pair : meters_) {
    res.emplace_back(pair.second);
  }
  return res;
}

void Registry::removed_expired_meters() noexcept {
  std::lock_guard<std::mutex> lock{meters_mutex};
  auto now = std::chrono::system_clock::now();
  auto it = meters_.begin();
  auto expired = 0;
  auto total = 0;
  while (it != meters_.end()) {
    ++total;
    if (is_meter_expired(now, *it->second, meter_ttl_)) {
      it = meters_.erase(it);
      ++expired;
    } else {
      ++it;
    }
  }
  logger_->debug("Removed {} expired meters out of {} total", expired, total);
}

void Registry::expirer() noexcept {
  if (meter_ttl_.count() == 0) {
    logger_->debug("Not expiring meters due to meter_ttl = 0");
    return;
  }

  auto freq_millis = config_->expiration_frequency;
  logger_->debug("Starting metrics expiration. Meter ttl={}s running every {}s",
                 meter_ttl_.count() / 1000, freq_millis.count() / 1000);

  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  while (!should_stop_) {
    auto start = clock::now();
    removed_expired_meters();
    auto elapsed = clock::now() - start;
    auto millis = duration_cast<milliseconds>(elapsed);
    if (millis < freq_millis) {
      std::unique_lock<std::mutex> lock{cv_mutex_};
      auto sleep = freq_millis - elapsed;
      logger_->debug("Expirer sleeping {}ms",
                     duration_cast<milliseconds>(sleep).count());
      cv_.wait_for(lock, sleep);
    }
  }
}

}  // namespace spectator
