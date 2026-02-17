#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace spectator {

/// Bounded lock-free multi-producer single-consumer queue.
/// Based on Dmitry Vyukov's bounded MPMC queue, simplified for single consumer.
/// Uses a ring buffer with sequence numbers to coordinate slot ownership.
template <typename T>
class MPSCQueue
{
   public:
    explicit MPSCQueue(size_t capacity) : m_capacity(NextPowerOf2(capacity)), m_mask(m_capacity - 1), m_slots(m_capacity)
    {
        for (size_t i = 0; i < m_capacity; ++i)
        {
            m_slots[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    /// Try to enqueue a value. Returns false if the queue is full.
    /// Lock-free and safe to call from multiple producer threads.
    bool TryPush(const T& value)
    {
        return PushImpl(value);
    }

    bool TryPush(T&& value)
    {
        return PushImpl(std::move(value));
    }

    /// Try to dequeue a value. Returns false if the queue is empty.
    /// Must only be called from a single consumer thread.
    bool TryPop(T& value)
    {
        Slot& slot = m_slots[m_dequeuePos & m_mask];
        size_t seq = slot.sequence.load(std::memory_order_acquire);
        if (seq != m_dequeuePos + 1)
        {
            return false;
        }
        value = std::move(slot.data);
        slot.sequence.store(m_dequeuePos + m_capacity, std::memory_order_release);
        m_dequeuePos++;
        return true;
    }

   private:
    struct Slot
    {
        std::atomic<size_t> sequence;
        T data;
    };

    template <typename U>
    bool PushImpl(U&& value)
    {
        while (true)
        {
            size_t pos = m_enqueuePos.load(std::memory_order_relaxed);
            Slot& slot = m_slots[pos & m_mask];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0)
            {
                if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    slot.data = std::forward<U>(value);
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0)
            {
                return false;  // queue full
            }
            // else: another producer claimed this slot, retry
        }
    }

    static size_t NextPowerOf2(size_t v)
    {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        return v + 1;
    }

    size_t m_capacity;
    size_t m_mask;
    std::vector<Slot> m_slots;

    // Separate cache lines to prevent false sharing between producers and consumer
    alignas(64) std::atomic<size_t> m_enqueuePos{0};
    alignas(64) size_t m_dequeuePos{0};
};

}  // namespace spectator
