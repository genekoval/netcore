#pragma once

#include "fd.h"
#include "system_event.hpp"

#include <chrono>
#include <fmt/format.h>

namespace netcore {
    class timer {
        friend struct fmt::formatter<timer>;

        fd descriptor;
        system_event event;

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
    };
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
            return format_to(
                ctx.out(),
                "timer ({})",
                static_cast<int>(timer.descriptor)
            );
        }
    };
}
