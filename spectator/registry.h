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
#include <tsl/hopscotch_map.h>

namespace spectator {
class Registry {
 public:
  using clock = std::chrono::steady_clock;
  using logger_ptr = std::shared_ptr<spdlog::logger>;

  Registry(std::unique_ptr<Config> config, logger_ptr logger) noexcept;
  ~Registry() noexcept { Stop(); }
  const Config& GetConfig() const noexcept;
  logger_ptr GetLogger() const noexcept;

  static IdPtr CreateId(std::string name, Tags tags) const noexcept;

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

  std::vector<std::shared_ptr<Meter>> Meters() const noexcept;
  std::vector<Measurement> Measurements() const noexcept;
  std::size_t Size() const noexcept {
    std::lock_guard<std::mutex> lock(meters_mutex);
    return meters_.size();
  }

  void Start() noexcept;
  void Stop() noexcept;

 private:
  std::atomic<bool> should_stop_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  std::thread expirer_thread_;
  std::chrono::milliseconds meter_ttl_;

  std::unique_ptr<Config> config_;
  logger_ptr logger_;
  mutable std::mutex meters_mutex{};
  using table_t = tsl::hopscotch_map<
      IdPtr, std::shared_ptr<Meter>, std::hash<IdPtr>, std::equal_to<IdPtr>,
      std::allocator<std::pair<IdPtr, std::shared_ptr<Meter>>>, 30, true>;
  table_t meters_;

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

  void expirer() noexcept;

  Publisher<Registry> publisher_;

 protected:
  // for testing
  void removed_expired_meters() noexcept;
};

}  // namespace spectator
