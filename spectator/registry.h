#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "config.h"
#include "logger.h"
#include "stateful_meters.h"
#include "stateless_meters.h"
#include "publisher.h"

namespace spectator {

// A registry for tests
// This is a stateful registry that will keep references to all registered
// meters and allows users to fetch the measurements at a later point

namespace detail {
inline void log_type_error(MeterType old_type, MeterType new_type,
                           const Id& id) {
  DefaultLogger()->warn(
      "Attempting to register {} as a {} but was previously registered as a {}",
      id, new_type, old_type);
}
}  // namespace detail

template <typename Types>
struct single_table_state {
  using types = Types;

  template <typename M, typename... Args>
  std::shared_ptr<M> get_or_create(IdPtr id, Args&&... args) {
    auto new_meter =
        std::make_shared<M>(std::move(id), std::forward<Args>(args)...);
    absl::MutexLock lock(&mutex_);
    auto it = meters_.find(new_meter->MeterId());
    if (it != meters_.end()) {
      // already exists, we need to ensure the existing type
      // matches the new meter type, otherwise we need to notify the user
      // of the error
      auto& old_meter = it->second;
      if (old_meter->GetType() != new_meter->GetType()) {
        detail::log_type_error(old_meter->GetType(), new_meter->GetType(),
                               *new_meter->MeterId());
        // this is not registered therefore no measurements
        // will be reported
        return new_meter;
      } else {
        return std::static_pointer_cast<M>(old_meter);
      }
    }

    meters_.emplace(new_meter->MeterId(), new_meter);
    return new_meter;
  }

  auto get_counter(IdPtr id) {
    return get_or_create<typename types::counter_t>(std::move(id));
  }

  auto get_gauge(IdPtr id) {
    return get_or_create<typename types::gauge_t>(std::move(id));
  }

  auto get_age_gauge(IdPtr id) {
    return get_or_create<typename types::age_gauge_t>(std::move(id));
  }

  auto get_max_gauge(IdPtr id) {
    return get_or_create<typename types::max_gauge_t>(std::move(id));
  }

  auto get_monotonic_counter(IdPtr id) {
    return get_or_create<typename types::monotonic_counter_t>(std::move(id));
  }

  auto get_timer(IdPtr id) {
    return get_or_create<typename types::timer_t>(std::move(id));
  }

  auto get_ds(IdPtr id) {
    return get_or_create<typename types::ds_t>(std::move(id));
  }

  auto get_perc_ds(IdPtr id, int64_t min, int64_t max) {
    return get_or_create<typename types::perc_ds_t>(std::move(id), min, max);
  }

  auto get_perc_timer(IdPtr id, std::chrono::nanoseconds min,
                      std::chrono::nanoseconds max) {
    return get_or_create<typename types::perc_timer_t>(std::move(id), min, max);
  }

  auto measurements() {
    std::vector<Measurement> result;

    absl::MutexLock lock(&mutex_);
    result.reserve(meters_.size() * 2);
    for (auto& m : meters_) {
      m.second->Measure(&result);
    }
    return result;
  }

  absl::Mutex mutex_;
  // use a single table so we can easily check whether a meter
  // was previously registered as a different type
  absl::flat_hash_map<IdPtr, std::shared_ptr<StatefulMeter>, std::hash<IdPtr>,
                      std::equal_to<IdPtr>>
      meters_ GUARDED_BY(mutex_);
};

template <typename State, typename Types = typename State::types>
class base_registry {
 public:
  using logger_ptr = std::shared_ptr<spdlog::logger>;
  using counter_t = typename Types::counter_t;
  using counter_ptr = std::shared_ptr<counter_t>;
  using dist_summary_t = typename Types::ds_t;
  using dist_summary_ptr = std::shared_ptr<dist_summary_t>;
  using gauge_t = typename Types::gauge_t;
  using gauge_ptr = std::shared_ptr<gauge_t>;
  using max_gauge_t = typename Types::max_gauge_t;
  using max_gauge_ptr = std::shared_ptr<max_gauge_t>;
  using age_gauge_t = typename Types::age_gauge_t;
  using age_gauge_ptr = std::shared_ptr<age_gauge_t>;
  using monotonic_counter_t = typename Types::monotonic_counter_t;
  using monotonic_counter_ptr = std::shared_ptr<monotonic_counter_t>;
  using perc_dist_summary_t = typename Types::perc_ds_t;
  using perc_dist_summary_ptr = std::shared_ptr<perc_dist_summary_t>;
  using perc_timer_t = typename Types::perc_timer_t;
  using perc_timer_ptr = std::shared_ptr<perc_timer_t>;
  using timer_t = typename Types::timer_t;
  using timer_ptr = std::shared_ptr<time_t>;

  explicit base_registry(logger_ptr logger = DefaultLogger())
      : logger_(std::move(logger)) {}

  auto GetCounter(const IdPtr& id) { return state_.get_counter(final_id(id)); }
  auto GetCounter(absl::string_view name, Tags tags = {}) {
    return GetCounter(Id::of(name, std::move(tags)));
  }

  auto GetDistributionSummary(const IdPtr& id) {
    return state_.get_ds(final_id(id));
  }
  auto GetDistributionSummary(absl::string_view name, Tags tags = {}) {
    return GetDistributionSummary(Id::of(name, std::move(tags)));
  }

  auto GetGauge(const IdPtr& id) { return state_.get_gauge(final_id(id)); }
  auto GetGauge(absl::string_view name, Tags tags = {}) {
    return GetGauge(Id::of(name, std::move(tags)));
  }

  auto GetMaxGauge(const IdPtr& id) {
    return state_.get_max_gauge(final_id(id));
  }
  auto GetMaxGauge(absl::string_view name, Tags tags = {}) {
    return GetMaxGauge(Id::of(name, std::move(tags)));
  }

  auto GetAgeGauge(const IdPtr& id) {
    return state_.get_age_gauge(final_id(id));
  }
  auto GetAgeGauge(absl::string_view name, Tags tags = {}) {
    return GetAgeGauge(Id::of(name, std::move(tags)));
  }

  auto GetMonotonicCounter(const IdPtr& id) {
    return state_.get_monotonic_counter(final_id(id));
  }
  auto GetMonotonicCounter(absl::string_view name, Tags tags = {}) {
    return GetMonotonicCounter(Id::of(name, std::move(tags)));
  }

  auto GetTimer(const IdPtr& id) { return state_.get_timer(final_id(id)); }
  auto GetTimer(absl::string_view name, Tags tags = {}) {
    return GetTimer(Id::of(name, std::move(tags)));
  }

  auto GetPercentileDistributionSummary(const IdPtr& id, int64_t min,
                                        int64_t max) {
    return state_.get_perc_ds(final_id(id), min, max);
  }

  auto GetPercentileDistributionSummary(absl::string_view name, int64_t min,
                                        int64_t max) {
    return GetPercentileDistributionSummary(Id::of(name), min, max);
  }

  auto GetPercentileDistributionSummary(absl::string_view name, Tags tags,
                                        int64_t min, int64_t max) {
    return GetPercentileDistributionSummary(Id::of(name, std::move(tags)), min,
                                            max);
  }

  auto GetPercentileTimer(const IdPtr& id, absl::Duration min,
                          absl::Duration max) {
    return state_.get_perc_timer(final_id(id), min, max);
  }

  auto GetPercentileTimer(const IdPtr& id, std::chrono::nanoseconds min,
                          std::chrono::nanoseconds max) {
    return state_.get_perc_timer(final_id(id), absl::FromChrono(min),
                                 absl::FromChrono(max));
  }

  auto GetPercentileTimer(absl::string_view name, absl::Duration min,
                          absl::Duration max) {
    return GetPercentileTimer(Id::of(name), min, max);
  }

  auto GetPercentileTimer(absl::string_view name, Tags tags, absl::Duration min,
                          absl::Duration max) {
    return GetPercentileTimer(Id::of(name, std::move(tags)), min, max);
  }

  auto GetPercentileTimer(absl::string_view name, std::chrono::nanoseconds min,
                          std::chrono::nanoseconds max) {
    return GetPercentileTimer(Id::of(name), absl::FromChrono(min),
                              absl::FromChrono(max));
  }

  auto GetPercentileTimer(absl::string_view name, Tags tags,
                          std::chrono::nanoseconds min,
                          std::chrono::nanoseconds max) {
    return GetPercentileTimer(Id::of(name, std::move(tags)),
                              absl::FromChrono(min), absl::FromChrono(max));
  }

  auto Measurements() { return state_.measurements(); }

 protected:
  logger_ptr logger_;
  State state_;
  Tags extra_tags_;

  // final Id after adding extra_tags_ if any
  IdPtr final_id(const IdPtr& id) {
    if (extra_tags_.size() > 0) {
      return id->WithTags(extra_tags_);
    }
    return id;
  }
};

template <typename Pub>
struct stateless_types {
  using counter_t = Counter<Pub>;
  using ds_t = DistributionSummary<Pub>;
  using gauge_t = Gauge<Pub>;
  using max_gauge_t = MaxGauge<Pub>;
  using age_gauge_t = AgeGauge<Pub>;
  using monotonic_counter_t = MonotonicCounter<Pub>;
  using perc_timer_t = PercentileTimer<Pub>;
  using perc_ds_t = PercentileDistributionSummary<Pub>;
  using timer_t = Timer<Pub>;
  using publisher_t = Pub;
};

template <typename Types>
struct stateless {
  using types = Types;
  std::unique_ptr<typename types::publisher_t> publisher;

  auto get_counter(IdPtr id) {
    return std::make_shared<typename types::counter_t>(std::move(id),
                                                       publisher.get());
  }

  auto get_gauge(IdPtr id) {
    return std::make_shared<typename types::gauge_t>(std::move(id),
                                                     publisher.get());
  }

  auto get_max_gauge(IdPtr id) {
    return std::make_shared<typename types::max_gauge_t>(std::move(id),
                                                         publisher.get());
  }

  auto get_age_gauge(IdPtr id) {
    return std::make_shared<typename types::age_gauge_t>(std::move(id),
                                                         publisher.get());
  }

  auto get_monotonic_counter(IdPtr id) {
    return std::make_shared<typename types::monotonic_counter_t>(
        std::move(id), publisher.get());
  }

  auto get_timer(IdPtr id) {
    return std::make_shared<typename types::timer_t>(std::move(id),
                                                     publisher.get());
  }

  auto get_ds(IdPtr id) {
    return std::make_shared<typename types::ds_t>(std::move(id),
                                                  publisher.get());
  }

  auto get_perc_ds(IdPtr id, int64_t min, int64_t max) {
    return std::make_shared<typename types::perc_ds_t>(
        std::move(id), publisher.get(), min, max);
  }

  auto get_perc_timer(IdPtr id, absl::Duration min, absl::Duration max) {
    return std::make_shared<typename types::perc_timer_t>(
        std::move(id), publisher.get(), min, max);
  }

  auto measurements() { return std::vector<Measurement>{}; }
};

/// A stateless registry that sends all meter activity immediately
/// to a spectatord agent
class SpectatordRegistry
    : public base_registry<stateless<stateless_types<SpectatordPublisher>>> {
 public:
  using types = stateless_types<SpectatordPublisher>;
  explicit SpectatordRegistry(const Config& config, logger_ptr logger)
      : base_registry<stateless<stateless_types<SpectatordPublisher>>>(
            std::move(logger)) {
    extra_tags_ = Tags::from(config.common_tags);
    state_.publisher =
        std::make_unique<SpectatordPublisher>(config.endpoint, logger_);
  }
};

/// A Registry that can be used for tests. It keeps state about which meters
/// have been registered, and can report the measurements from all the
/// registered meters
struct TestRegistry : base_registry<single_table_state<stateful_meters>> {
  using types = stateful_meters;
};

/// The default registry
using Registry = SpectatordRegistry;

}  // namespace spectator
