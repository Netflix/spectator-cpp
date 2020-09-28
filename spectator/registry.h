#pragma once

#include "config.h"
#include "counter.h"
#include "dist_summary.h"
#include "gauge.h"
#include "logger.h"
#include "max_gauge.h"
#include "monotonic_counter.h"
#include "publisher.h"
#include "timer.h"
#include "percentile_timer.h"
#include "percentile_distribution_summary.h"

namespace spectator {
class Registry {
 public:
  using logger_ptr = std::shared_ptr<spdlog::logger>;
  Registry(Config config, logger_ptr logger) noexcept;
  const Config& GetConfig() const noexcept;
  logger_ptr GetLogger() const noexcept;

  IdPtr CreateId(std::string name, Tags tags) const noexcept;

  std::shared_ptr<Counter> GetCounter(IdPtr id) noexcept;
  std::shared_ptr<Counter> GetCounter(std::string name,
                                      Tags tags = {}) noexcept;

  std::shared_ptr<MonotonicCounter> GetMonotonicCounter(IdPtr id) noexcept;
  std::shared_ptr<MonotonicCounter> GetMonotonicCounter(
      std::string name, Tags tags = {}) noexcept;

  std::shared_ptr<DistributionSummary> GetDistributionSummary(
      IdPtr id) noexcept;
  std::shared_ptr<DistributionSummary> GetDistributionSummary(
      std::string name, Tags tags = {}) noexcept;

  std::shared_ptr<Gauge> GetGauge(IdPtr id) noexcept;
  std::shared_ptr<Gauge> GetGauge(std::string name, Tags tags = {}) noexcept;

  std::shared_ptr<MaxGauge> GetMaxGauge(IdPtr id) noexcept;
  std::shared_ptr<MaxGauge> GetMaxGauge(std::string name,
                                        Tags tags = {}) noexcept;

  std::shared_ptr<Timer> GetTimer(IdPtr id) noexcept;
  std::shared_ptr<Timer> GetTimer(std::string name, Tags tags = {}) noexcept;

  std::shared_ptr<PercentileDistributionSummary>
  GetPercentileDistributionSummary(IdPtr id, int64_t min, int64_t max) noexcept;

  std::shared_ptr<PercentileTimer> GetPercentileTimer(
      IdPtr id, absl::Duration min, absl::Duration max) noexcept;

  std::shared_ptr<PercentileTimer> GetPercentileTimer(
      IdPtr id, std::chrono::nanoseconds min,
      std::chrono::nanoseconds max) noexcept;

 private:
  Config config_;
  logger_ptr logger_;

 protected:
  // for testing
  Registry(Config config, logger_ptr logger,
           std::unique_ptr<Publisher> publisher)
      : config_(std::move(config)),
        logger_(std::move(logger)),
        publisher_(std::move(publisher)) {}
  std::unique_ptr<Publisher> publisher_;
};

}  // namespace spectator
