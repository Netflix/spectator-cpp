#include "counter.h"
#include "atomicnumber.h"

namespace spectator {

Counter::Counter(IdPtr id) noexcept : id_{std::move(id)}, count_{0.0} {}

IdPtr Counter::MeterId() const noexcept { return id_; }

std::vector<Measurement> Counter::Measure() const noexcept {
  auto count = count_.exchange(0.0, std::memory_order_relaxed);
  if (count > 0) {
    return std::vector<Measurement>({{id_->WithStat("count"), count}});
  }
  return std::vector<Measurement>();
}

void Counter::Increment() noexcept { Add(1.0); }

void Counter::Add(double delta) noexcept {
  if (delta < 0) {
    return;
  }

  add_double(&count_, delta);
}

double Counter::Count() const noexcept {
  return count_.load(std::memory_order_relaxed);
}

}  // namespace spectator
