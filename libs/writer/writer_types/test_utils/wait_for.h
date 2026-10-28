#pragma once

#include <chrono>
#include <thread>

namespace spectator::test {

// Polls predicate until it returns true or the timeout passes, and returns its last result. Tests use it instead of
// fixed sleeps, so they finish as soon as the condition holds and stay reliable on a slow machine.
template <typename Predicate>
bool WaitFor(Predicate&& predicate, std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return predicate();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

}  // namespace spectator::test
