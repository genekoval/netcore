#pragma once

#include <netcore/timer.hpp>

#include <chrono>
#include <ext/coroutine>
#include <span>

namespace netcore {
    struct async_options {
        int max_events;
        std::chrono::seconds timeout;
        std::span<const int> signals;
    };

    auto async(ext::task<>&& task) -> void;

    auto async(const async_options& options, ext::task<>&& task) -> void;

    template <typename Rep, typename Period>
    auto sleep_for(
        const std::chrono::duration<Rep, Period>& duration
    ) -> ext::task<> {
        auto timer = netcore::timer::monotonic();
        timer.set(duration);

        co_await timer.wait();
    }

    template <typename Clock, typename Duration>
    auto sleep_until(
        const std::chrono::time_point<Clock, Duration>& sleep_time
    ) -> ext::task<> {
        const auto duration = sleep_time - Clock::now();

        auto timer = netcore::timer::realtime();
        timer.set(duration);

        co_await timer.wait();
    }

    auto yield() -> ext::task<>;
}
