#pragma once

#include <atomic>

namespace spectator {

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

}  // namespace spectator
