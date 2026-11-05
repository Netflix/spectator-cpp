#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace perf {

// Counts values, such as durations in nanoseconds, in buckets that get wider as the values get larger: every value
// below 64 has a bucket of its own, and every power of two above that is split into 32 buckets, so a value is off by
// about 3% at most. Recording a value is one array increment and never allocates, which keeps the measurement out of
// the way of what it measures.
class Histogram
{
   public:
    void Record(std::uint64_t value) noexcept
    {
        value = std::min(value, kLargestValue);
        ++m_buckets[BucketOf(value)];
        ++m_count;
        m_max = std::max(m_max, value);
    }

    void Merge(const Histogram& other) noexcept
    {
        for (std::size_t bucket = 0; bucket < m_buckets.size(); ++bucket)
        {
            m_buckets[bucket] += other.m_buckets[bucket];
        }
        m_count += other.m_count;
        m_max = std::max(m_max, other.m_max);
    }

    std::uint64_t Count() const noexcept { return m_count; }

    // The largest value recorded, or zero if nothing was recorded.
    std::uint64_t Max() const noexcept { return m_max; }

    // The value that the given share of the recorded values does not exceed, for example 0.99 for the 99th
    // percentile. Zero if nothing was recorded.
    std::uint64_t Percentile(double share) const noexcept
    {
        if (m_count == 0)
        {
            return 0;
        }
        const auto wanted = std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(std::ceil(share * static_cast<double>(m_count))));
        std::uint64_t seen = 0;
        for (std::size_t bucket = 0; bucket < m_buckets.size(); ++bucket)
        {
            seen += m_buckets[bucket];
            if (seen >= wanted)
            {
                return std::min(LargestValueOf(bucket), m_max);
            }
        }
        return m_max;
    }

   private:
    static constexpr int kSplitBits = 5;                                         // 32 buckets per power of two
    static constexpr std::uint64_t kSplit = std::uint64_t{1} << kSplitBits;      // 32
    static constexpr std::uint64_t kExactBelow = kSplit * 2;                     // 64
    static constexpr int kLargestPower = 41;                                     // 2^42 ns is over an hour
    static constexpr std::uint64_t kLargestValue = (std::uint64_t{1} << (kLargestPower + 1)) - 1;
    static constexpr std::size_t kBuckets = kExactBelow + (kLargestPower - kSplitBits) * kSplit;

    static std::size_t BucketOf(std::uint64_t value) noexcept
    {
        if (value < kExactBelow)
        {
            return static_cast<std::size_t>(value);
        }
        const int power = std::bit_width(value) - 1;  // 6 or more
        const int shift = power - kSplitBits;
        const std::uint64_t withinPower = (value >> shift) - kSplit;  // 0 to 31
        return static_cast<std::size_t>(kExactBelow + static_cast<std::uint64_t>(power - kSplitBits - 1) * kSplit +
                                        withinPower);
    }

    static std::uint64_t LargestValueOf(std::size_t bucket) noexcept
    {
        if (bucket < kExactBelow)
        {
            return bucket;
        }
        const std::uint64_t above = bucket - kExactBelow;
        const int power = static_cast<int>(above / kSplit) + kSplitBits + 1;
        const int shift = power - kSplitBits;
        return ((kSplit + above % kSplit + 1) << shift) - 1;
    }

    std::array<std::uint64_t, kBuckets> m_buckets{};
    std::uint64_t m_count = 0;
    std::uint64_t m_max = 0;
};

}  // namespace perf
