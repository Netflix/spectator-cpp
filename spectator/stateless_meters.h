#pragma once
#include "id.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

namespace spectator {

namespace detail {
inline std::string as_string(std::string_view v) { 
    return {v.data(), v.size()}; 
}
inline std::string create_prefix(const Id& id, std::string_view type_name) {
  std::string res = as_string(type_name) + ":" + id.Name();
  for (const auto& tags : id.GetTags()) {
    absl::StrAppend(&res, ",", tags.first, "=", tags.second);
  }
  absl::StrAppend(&res, ":");
  return res;
}

template <typename T>
T restrict(T amount, T min, T max) {
  auto r = amount;
  if (r > max) {
    r = max;
  } else if (r < min) {
    r = min;
  }
  return r;
}
}  // namespace detail

template <typename Pub>
class StatelessMeter {
 public:
  StatelessMeter(IdPtr id, Pub* publisher)
      : id_(std::move(id)), publisher_(publisher) {
    assert(publisher_ != nullptr);
  }
  virtual ~StatelessMeter() = default;
  [[nodiscard]] IdPtr MeterId() const noexcept { return id_; }
  [[nodiscard]] virtual std::string_view Type() = 0;

 protected:
  void send(double value) {
    if (value_prefix_.empty()) {
      value_prefix_ = detail::create_prefix(*id_, Type());
    }
    auto msg = absl::StrFormat("%s%f", value_prefix_, value);
    // remove trailing zeros and decimal points
    msg.erase(msg.find_last_not_of('0') + 1, std::string::npos);
    msg.erase(msg.find_last_not_of('.') + 1, std::string::npos);
    publisher_->send(msg);
  }

 private:
  IdPtr id_;
  Pub* publisher_;
  std::string value_prefix_;
};

template <typename Pub>
class AgeGauge : public StatelessMeter<Pub> {
 public:
  AgeGauge(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Now() noexcept { this->send(0); }
  void Set(double value) noexcept { this->send(value); }

 protected:
  std::string_view Type() override { return "A"; }
};

template <typename Pub>
class Counter : public StatelessMeter<Pub> {
 public:
  Counter(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Increment() noexcept { this->send(1); };
  void Add(double delta) noexcept { this->send(delta); }

 protected:
  std::string_view Type() override { return "c"; }
};

template <typename Pub>
class DistributionSummary : public StatelessMeter<Pub> {
 public:
  DistributionSummary(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Record(double amount) noexcept { this->send(amount); }

 protected:
  std::string_view Type() override { return "d"; }
};

template <typename Pub>
class Gauge : public StatelessMeter<Pub> {
 public:
  Gauge(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Set(double value) noexcept { this->send(value); }

 protected:
  std::string_view Type() override { return "g"; }
};

template <typename Pub>
class MaxGauge : public StatelessMeter<Pub> {
 public:
  MaxGauge(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Update(double value) noexcept { this->send(value); }
  // synonym for Update for consistency with the Gauge interface
  void Set(double value) noexcept { this->send(value); }

 protected:
  std::string_view Type() override { return "m"; }
};

template <typename Pub>
class MonotonicCounter : public StatelessMeter<Pub> {
 public:
  MonotonicCounter(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Set(double amount) noexcept { this->send(amount); }

 protected:
  std::string_view Type() override { return "C"; }
};

template <typename Pub>
class MonotonicCounterUint : public StatelessMeter<Pub> {
 public:
  MonotonicCounterUint(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Set(uint64_t amount) noexcept { this->send(amount); }

 protected:
  std::string_view Type() override { return "U"; }
};

template <typename Pub>
class PercentileDistributionSummary : public StatelessMeter<Pub> {
 public:
  PercentileDistributionSummary(IdPtr id, Pub* publisher, int64_t min,
                                int64_t max)
      : StatelessMeter<Pub>(std::move(id), publisher), min_{min}, max_{max} {}

  void Record(int64_t amount) noexcept {
    this->send(detail::restrict(amount, min_, max_));
  }

 protected:
  std::string_view Type() override { return "D"; }

 private:
  int64_t min_;
  int64_t max_;
};

template <typename Pub>
class PercentileTimer : public StatelessMeter<Pub> {
 public:
  PercentileTimer(IdPtr id, Pub* publisher, absl::Duration min,
                  absl::Duration max)
      : StatelessMeter<Pub>(std::move(id), publisher), min_(min), max_(max) {}

  PercentileTimer(IdPtr id, Pub* publisher, std::chrono::nanoseconds min,
                  std::chrono::nanoseconds max)
      : PercentileTimer(std::move(id), publisher, absl::FromChrono(min),
                        absl::FromChrono(max)) {}

  void Record(std::chrono::nanoseconds amount) noexcept {
    Record(absl::FromChrono(amount));
  }

  void Record(absl::Duration amount) noexcept {
    auto duration = detail::restrict(amount, min_, max_);
    this->send(absl::ToDoubleSeconds(duration));
  }

 protected:
  std::string_view Type() override { return "T"; }

 private:
  absl::Duration min_;
  absl::Duration max_;
};

template <typename Pub>
class Timer : public StatelessMeter<Pub> {
 public:
  Timer(IdPtr id, Pub* publisher)
      : StatelessMeter<Pub>(std::move(id), publisher) {}
  void Record(std::chrono::nanoseconds amount) noexcept {
    Record(absl::FromChrono(amount));
  }

  void Record(absl::Duration amount) noexcept {
    auto secs = absl::ToDoubleSeconds(amount);
    this->send(secs);
  }

 protected:
  std::string_view Type() override { return "t"; }
};

}  // namespace spectator
