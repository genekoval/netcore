#pragma once

#include "detail/awaiter.hpp"

#include <cstdint>
#include <ext/coroutine>

namespace netcore {
    class runtime;

    class system_event {
        friend class runtime;

        int fd = -1;
        uint32_t events = 0;
        bool registered = false;
        detail::awaiter_queue awaiters;

        auto resume() -> void;
    public:
        system_event() = default;

        system_event(int fd, uint32_t events);

        system_event(const system_event&) = delete;

        system_event(system_event&& other);

        auto operator=(const system_event&) -> system_event& = delete;

        auto operator=(system_event&&) -> system_event&;

        auto cancel() -> void;

        auto error(std::exception_ptr ex) noexcept -> void;

        auto latest_events() const noexcept -> uint32_t;

        auto notify() -> void;

        template <typename T>
        auto notify(const T state) -> void {
            awaiters.assign(state);
            notify();
        }

        auto wait(uint32_t events, void* state) -> ext::task<detail::awaiter*>;
    };
}
