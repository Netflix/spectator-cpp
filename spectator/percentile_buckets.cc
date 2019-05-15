#include "percentile_buckets.h"
#include <limits>

namespace spectator {

#include "percentile_bucket_values.inc"

int64_t GetPercBucketValue(size_t i) { return kBucketValues.at(i); }

size_t PercentileBucketIndexOf(int64_t v) {
  if (v <= 0) {
    return 0;
  }
  if (v <= 4) {
    return static_cast<size_t>(v);
  }
  size_t lz = __builtin_clzll(v);
  size_t shift = 64 - lz - 1;
  int64_t prevPowerOf2 = (v >> shift) << shift;
  int64_t prevPowerOf4 = prevPowerOf2;
  if (shift % 2 != 0) {
    --shift;
    prevPowerOf4 >>= 1;
  }

  auto base = prevPowerOf4;
  auto delta = base / 3;
  auto offset = static_cast<size_t>((v - base) / delta);
  size_t pos = offset + kPowerOf4Index.at(shift / 2);
  return pos >= kBucketValues.size() - 1 ? kBucketValues.size() - 1 : pos + 1;
}

void Percentiles(const std::array<int64_t, PercentileBucketsLength()>& counts,
                 const std::vector<double>& pcts,
                 std::vector<double>* results) {
  int64_t total = 0;
  for (size_t i = 0; i < kBucketValues.size(); ++i) {
    total += counts.at(i);
  }

  size_t pctIdx = 0;
  int64_t prev = 0;
  auto prevP = 0.0;
  int64_t prevB = 0;
  results->clear();
  results->resize(pcts.size());

  for (size_t i = 0; i < kBucketValues.size(); ++i) {
    auto next = prev + counts.at(i);
    auto nextP = 100.0 * next / total;
    auto nextB = kBucketValues.at(i);
    while (pctIdx < pcts.size() && nextP >= pcts[pctIdx]) {
      auto f = (pcts[pctIdx] - prevP) / (nextP - prevP);
      results->at(pctIdx) = f * (nextB - prevB) + prevB;
      ++pctIdx;
    }
    if (pctIdx >= pcts.size()) {
      break;
    }
    prev = next;
    prevP = nextP;
    prevB = nextB;
  }

  auto nextP = 100.0;
  auto nextB = std::numeric_limits<int64_t>::max();
  while (pctIdx < pcts.size()) {
    auto f = (pcts[pctIdx] - prevP) / (nextP - prevP);
    results->at(pctIdx) = f * (nextB - prevB) + prevB;
    ++pctIdx;
  }
}

double Percentile(const std::array<int64_t, PercentileBucketsLength()>& counts,
                  double p) {
  std::vector<double> pcts{p};
  std::vector<double> results;
  Percentiles(counts, pcts, &results);
  return results[0];
}

int64_t PercentileBucket(int64_t v) {
  return GetPercBucketValue(PercentileBucketIndexOf(v));
}

}  // namespace spectator
