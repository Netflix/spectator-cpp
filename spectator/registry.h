#pragma once

#include "config.h"
#include "counter.h"
#include "dist_summary.h"
#include "gauge.h"
#include "max_gauge.h"
#include "monotonic_counter.h"
#include "publisher.h"
#include "timer.h"
#include <mutex>
#include <ska/flat_hash_map.hpp>

namespace spectator {
class Registry {
 public:
  using clock = std::chrono::steady_clock;
  using logger_ptr = std::shared_ptr<spdlog::logger>;

  Registry(std::unique_ptr<Config> config, logger_ptr logger) noexcept;
  const Config& GetConfig() const noexcept;
  logger_ptr GetLogger() const noexcept;

  IdPtr CreateId(std::string name, Tags tags) const noexcept;

  std::shared_ptr<Counter> GetCounter(IdPtr id) noexcept;
  std::shared_ptr<Counter> GetCounter(std::string name) noexcept;

  std::shared_ptr<MonotonicCounter> GetMonotonicCounter(IdPtr id) noexcept;
  std::shared_ptr<MonotonicCounter> GetMonotonicCounter(
      std::string name) noexcept;

  std::shared_ptr<DistributionSummary> GetDistributionSummary(
      IdPtr id) noexcept;
  std::shared_ptr<DistributionSummary> GetDistributionSummary(
      std::string name) noexcept;

  std::shared_ptr<Gauge> GetGauge(IdPtr id) noexcept;
  std::shared_ptr<Gauge> GetGauge(std::string name) noexcept;

  std::shared_ptr<MaxGauge> GetMaxGauge(IdPtr id) noexcept;
  std::shared_ptr<MaxGauge> GetMaxGauge(std::string name) noexcept;

  std::shared_ptr<Timer> GetTimer(IdPtr id) noexcept;
  std::shared_ptr<Timer> GetTimer(std::string name) noexcept;

  std::vector<std::shared_ptr<Meter>> Meters() const noexcept;
  std::vector<Measurement> Measurements() const noexcept;

  void Start() noexcept;
  void Stop() noexcept;

 private:
  std::unique_ptr<Config> config_;
  logger_ptr logger_;
  mutable std::mutex meters_mutex{};
  ska::flat_hash_map<IdPtr, std::shared_ptr<Meter>> meters_;

  std::shared_ptr<Meter> insert_if_needed(
      std::shared_ptr<Meter> meter) noexcept;
  void log_type_error(const Id& id, MeterType prev_type,
                      MeterType attempted_type) const noexcept;

  template <typename M, typename... Args>
  std::shared_ptr<M> create_and_register_as_needed(IdPtr id,
                                                   Args&&... args) noexcept {
    std::shared_ptr<M> new_meter_ptr{
        std::make_shared<M>(std::move(id), std::forward<Args>(args)...)};
    auto meter_ptr = insert_if_needed(new_meter_ptr);
    if (meter_ptr->GetType() != new_meter_ptr->GetType()) {
      log_type_error(*meter_ptr->MeterId(), meter_ptr->GetType(),
                     new_meter_ptr->GetType());
      return new_meter_ptr;
    }
    return std::static_pointer_cast<M>(meter_ptr);
  }

  Publisher<Registry> publisher_;
};

}  // namespace spectator
