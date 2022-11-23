#pragma once

#include <netcore/detail/notifier.hpp>

#include <chrono>
#include <sys/timerfd.h>

namespace netcore {
    class timer {
        fd descriptor;
        detail::notification notification;

        auto get_time() const -> itimerspec;

        auto set_time(long value, long interval) const -> void;

        timer(int clockid);
    public:
        using nanoseconds = std::chrono::nanoseconds;

        static auto realtime() -> timer;

        static auto monotonic() -> timer;

        static auto boottime() -> timer;

        static auto realtime_alarm() -> timer;

        static auto boottime_alarm() -> timer;

        timer() = default;

        auto armed() const -> bool;

        auto disarm() -> void;

        auto set(
            nanoseconds nsec,
            nanoseconds interval = nanoseconds::zero()
        ) -> void;

        auto wait() -> ext::task<std::uint64_t>;
    };
}
