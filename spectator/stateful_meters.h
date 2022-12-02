#pragma once

#include "id.h"
#include "measurement.h"
#include "meter_type.h"

namespace spectator {

namespace detail {
/// Atomically add a delta to an atomic double
/// equivalent to fetch_add for integer types
inline void add_double(std::atomic<double>* n, double delta) {
  double current;
  do {
    current = n->load(std::memory_order_relaxed);
  } while (!n->compare_exchange_weak(
      current, n->load(std::memory_order_relaxed) + delta));
}

/// Atomically set the max value of an atomic number
template <typename T>
inline void update_max(std::atomic<T>* n, T value) {
  T current;
  do {
    current = n->load(std::memory_order_relaxed);
  } while (value > current && !n->compare_exchange_weak(current, value));
}
}  // namespace detail

class StatefulMeter {
 public:
  explicit StatefulMeter(IdPtr id) : id_{std::move(id)} {}
  StatefulMeter(const StatefulMeter&) = default;
  virtual ~StatefulMeter() = default;
  virtual void Measure(std::vector<Measurement>* measurements) = 0;
  [[nodiscard]] virtual MeterType GetType() const = 0;
  [[nodiscard]] IdPtr MeterId() const { return id_; }

 protected:
  IdPtr id_;
};

class StatefulCounter : public StatefulMeter {
 public:
  explicit StatefulCounter(IdPtr id) : StatefulMeter(std::move(id)) {}
  [[nodiscard]] double Count() const { return count_; };

  MeterType GetType() const override { return MeterType::Counter; }

  void Add(double delta) {
    if (delta > 0) {
      detail::add_double(&count_, delta);
    }
  }
  void Increment() { Add(1); }

  void Measure(std::vector<Measurement>* measurements) override {
    auto count = count_.exchange(0.0);
    if (count > 0) {
      measurements->emplace_back(Id::WithDefaultStat(id_, "count"), count);
    }
  }

 private:
  std::atomic<double> count_ = 0.0;
};

template <typename DistType>
class TestDistribution : public StatefulMeter {
 public:
  explicit TestDistribution(IdPtr id) : StatefulMeter(std::move(id)) {}
  int64_t Count() const { return count_; }
  double TotalAmount() const { return total_; }
  MeterType GetType() const override { return DistType::meter_type; }

  void Measure(std::vector<Measurement>* measurements) override {
    auto cnt = count_.exchange(0);
    if (cnt == 0) {
      return;
    }
    auto total = total_.exchange(0);
    auto t_sq = totalSq_.exchange(0);
    auto mx = max_.exchange(0);
    measurements->emplace_back(id_->WithStat(DistType::total_name), total);
    measurements->emplace_back(id_->WithStat("totalOfSquares"), t_sq);
    measurements->emplace_back(id_->WithStat("max"), mx);
    measurements->emplace_back(id_->WithStat("count"), cnt);
  }

 protected:
  void record(double amount) {
    if (amount >= 0) {
      count_.fetch_add(1);
      detail::add_double(&total_, amount);
      detail::add_double(&totalSq_, amount * amount);
      detail::update_max(&max_, amount);
    }
  }

 private:
  std::atomic<int64_t> count_ = 0;
  std::atomic<double> total_ = 0;
  std::atomic<double> totalSq_ = 0;
  std::atomic<double> max_ = 0;
};

struct timer_distribution {
  static constexpr auto meter_type = MeterType::Timer;
  static constexpr auto total_name = "totalTime";
};

struct summary_distribution {
  static constexpr auto meter_type = MeterType::DistSummary;
  static constexpr auto total_name = "totalAmount";
};

class StatefulTimer : public TestDistribution<timer_distribution> {
 public:
  explicit StatefulTimer(IdPtr id)
      : TestDistribution<timer_distribution>(std::move(id)) {}

  void Record(absl::Duration amount) { record(absl::ToDoubleSeconds(amount)); }

  void Record(std::chrono::nanoseconds amount) {
    Record(absl::FromChrono(amount));
  }
};

class StatefulDistSum : public TestDistribution<summary_distribution> {
 public:
  explicit StatefulDistSum(IdPtr id)
      : TestDistribution<summary_distribution>(std::move(id)) {}
  void Record(double amount) { record(amount); }
};

class StatefulGauge : public StatefulMeter {
 public:
  explicit StatefulGauge(IdPtr id) : StatefulMeter(std::move(id)) {}
  [[nodiscard]] double Get() const { return value_; }
  MeterType GetType() const override { return MeterType::Gauge; }
  void Set(double amount) { value_ = amount; }
  void Measure(std::vector<Measurement>* measurements) override {
    auto v = value_.exchange(kNaN);
    if (std::isnan(v)) {
      return;
    }
    measurements->emplace_back(Id::WithDefaultStat(id_, "gauge"), v);
  }

 private:
  static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();
  std::atomic<double> value_ = kNaN;
};

class StatefulMaxGauge : public StatefulMeter {
 public:
  explicit StatefulMaxGauge(IdPtr id) : StatefulMeter(std::move(id)) {}
  [[nodiscard]] double Get() const { return value_; }
  MeterType GetType() const override { return MeterType::MaxGauge; }
  void Set(double amount) { detail::update_max(&value_, amount); }
  void Update(double amount) { Set(amount); }
  void Measure(std::vector<Measurement>* measurements) override {
    auto v = value_.exchange(kMinValue);
    if (v == kMinValue) {
      return;
    }
    measurements->emplace_back(Id::WithDefaultStat(id_, "max"), v);
  }

 private:
  static constexpr auto kMinValue = std::numeric_limits<double>::lowest();
  std::atomic<double> value_ = kMinValue;
};

class StatefulMonoCounter : public StatefulMeter {
 public:
  explicit StatefulMonoCounter(IdPtr id) : StatefulMeter(std::move(id)) {}
  MeterType GetType() const override { return MeterType::MonotonicCounter; }
  [[nodiscard]] double Delta() const { return value_ - prev_value_; }
  void Set(double amount) { value_ = amount; }
  void Measure(std::vector<Measurement>* measurements) override {
    auto delta = Delta();
    prev_value_ = value_.load();
    if (delta > 0) {
      measurements->emplace_back(id_->WithStat("count"), delta);
    }
  }

 private:
  static constexpr auto kNaN = std::numeric_limits<double>::quiet_NaN();
  std::atomic<double> value_ = kNaN;
  std::atomic<double> prev_value_ = kNaN;
};

class StatefulPercTimer : public StatefulMeter {
 public:
  StatefulPercTimer(IdPtr id, std::chrono::nanoseconds,
                    std::chrono::nanoseconds)
      : StatefulMeter(std::move(id)) {}
  MeterType GetType() const override { return MeterType::PercentileTimer; }
  void Measure(std::vector<Measurement>*) override {}

 private:
};

class StatefulPercDistSum : public StatefulMeter {
 public:
  StatefulPercDistSum(IdPtr id, int64_t, int64_t)
      : StatefulMeter(std::move(id)) {}
  MeterType GetType() const override {
    return MeterType::PercentileDistSummary;
  }
  void Measure(std::vector<Measurement>*) override {}
};

struct stateful_meters {
  using counter_t = StatefulCounter;
  using ds_t = StatefulDistSum;
  using gauge_t = StatefulGauge;
  using max_gauge_t = StatefulMaxGauge;
  using monotonic_counter_t = StatefulMonoCounter;
  using perc_timer_t = StatefulPercTimer;
  using perc_ds_t = StatefulPercDistSum;
  using timer_t = StatefulTimer;
};

}  // namespace spectator
