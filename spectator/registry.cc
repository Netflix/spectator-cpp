#include "logger.h"
#include "registry.h"

namespace spectator {

Registry::Registry(Config config, Registry::logger_ptr logger) noexcept
    : config_{std::move(config)},
      logger_{std::move(logger)},
      publisher_(std::make_unique<Publisher>(config_.endpoint)) {}

const Config& Registry::GetConfig() const noexcept { return config_; }

Registry::logger_ptr Registry::GetLogger() const noexcept { return logger_; }

IdPtr Registry::CreateId(std::string name, Tags tags) const noexcept {
  return std::make_shared<Id>(name, tags);
}

std::shared_ptr<Counter> Registry::GetCounter(IdPtr id) noexcept {
  return std::make_shared<Counter>(std::move(id), publisher_.get());
}

std::shared_ptr<Counter> Registry::GetCounter(std::string name,
                                              Tags tags) noexcept {
  return GetCounter(CreateId(std::move(name), std::move(tags)));
}

std::shared_ptr<DistributionSummary> Registry::GetDistributionSummary(
    IdPtr id) noexcept {
  return std::make_shared<DistributionSummary>(std::move(id), publisher_.get());
}

std::shared_ptr<DistributionSummary> Registry::GetDistributionSummary(
    std::string name, Tags tags) noexcept {
  return GetDistributionSummary(CreateId(std::move(name), std::move(tags)));
}

std::shared_ptr<Gauge> Registry::GetGauge(IdPtr id) noexcept {
  return std::make_shared<Gauge>(std::move(id), publisher_.get());
}

std::shared_ptr<Gauge> Registry::GetGauge(std::string name,
                                          Tags tags) noexcept {
  return GetGauge(CreateId(std::move(name), std::move(tags)));
}

std::shared_ptr<MaxGauge> Registry::GetMaxGauge(IdPtr id) noexcept {
  return std::make_shared<MaxGauge>(std::move(id), publisher_.get());
}

std::shared_ptr<MaxGauge> Registry::GetMaxGauge(std::string name,
                                                Tags tags) noexcept {
  return GetMaxGauge(CreateId(std::move(name), std::move(tags)));
}

std::shared_ptr<MonotonicCounter> Registry::GetMonotonicCounter(
    IdPtr id) noexcept {
  return std::make_shared<MonotonicCounter>(std::move(id), publisher_.get());
}

std::shared_ptr<MonotonicCounter> Registry::GetMonotonicCounter(
    std::string name, Tags tags) noexcept {
  return GetMonotonicCounter(CreateId(std::move(name), std::move(tags)));
}

std::shared_ptr<Timer> Registry::GetTimer(IdPtr id) noexcept {
  return std::make_shared<Timer>(std::move(id), publisher_.get());
}

std::shared_ptr<Timer> Registry::GetTimer(std::string name,
                                          Tags tags) noexcept {
  return GetTimer(CreateId(std::move(name), std::move(tags)));
}

std::shared_ptr<PercentileDistributionSummary>
Registry::GetPercentileDistributionSummary(IdPtr id, int64_t min,
                                           int64_t max) noexcept {
  return std::make_shared<PercentileDistributionSummary>(
      std::move(id), publisher_.get(), min, max);
}

std::shared_ptr<PercentileTimer> Registry::GetPercentileTimer(
    IdPtr id, absl::Duration min, absl::Duration max) noexcept {
  return std::make_shared<PercentileTimer>(std::move(id), publisher_.get(), min,
                                           max);
}

std::shared_ptr<PercentileTimer> Registry::GetPercentileTimer(
    IdPtr id, std::chrono::nanoseconds min,
    std::chrono::nanoseconds max) noexcept {
  return std::make_shared<PercentileTimer>(std::move(id), publisher_.get(), min,
                                           max);
}

}  // namespace spectator
