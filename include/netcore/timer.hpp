#pragma once

#include "fd.hpp"
#include "runtime.hpp"

#include <chrono>
#include <fmt/format.h>

namespace netcore {
    class timer {
        friend struct fmt::formatter<timer>;

        fd descriptor;
        std::shared_ptr<runtime::event> event;

        timer(int clockid);
    public:
        struct time {
            std::chrono::nanoseconds value;
            std::chrono::nanoseconds interval;
        };

        static auto realtime() -> timer;

        static auto monotonic() -> timer;

        static auto boottime() -> timer;

        static auto realtime_alarm() -> timer;

        static auto boottime_alarm() -> timer;

        timer() = default;

        auto disarm() -> void;

        auto get() const -> time;

        auto set(std::chrono::nanoseconds value) const -> void;

        auto set(const time& t) const -> void;

        auto wait() -> ext::task<std::uint64_t>;

        auto waiting() const noexcept -> bool;
    };

    template <typename Rep, typename Period>
    auto sleep_for(const std::chrono::duration<Rep, Period>& duration)
        -> ext::task<> {
        auto timer = netcore::timer::monotonic();
        timer.set(duration);

        co_await timer.wait();
    }

    template <typename Clock, typename Duration>
    auto sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time)
        -> ext::task<> {
        const auto duration = sleep_time - Clock::now();

        auto timer = netcore::timer::realtime();
        timer.set(duration);

        co_await timer.wait();
    }
}

namespace fmt {
    template <>
    struct formatter<netcore::timer> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const netcore::timer& timer, FormatContext& ctx) {
            return fmt::format_to(
                ctx.out(),
                "timer ({})",
                static_cast<int>(timer.descriptor)
            );
        }
    };
}
