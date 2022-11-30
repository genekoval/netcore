#pragma once

#include "detail/awaiter.hpp"

#include <cstdint>
#include <ext/coroutine>

namespace netcore {
    class runtime;
    class system_event;

    class [[nodiscard]] register_guard {
        friend class system_event;

        system_event& notif;

        explicit register_guard(system_event& event);
    public:
        ~register_guard();
    };

    class system_event {
        friend class runtime;

        int fd = -1;
        uint32_t events = 0;
        system_event* head = this;
        system_event* tail = this;
        detail::awaiter_queue awaiters;
        runtime* last_runtime = nullptr;

        auto append(system_event& other) noexcept -> void;

        auto empty() const noexcept -> bool;

        auto one_or_empty() const noexcept -> bool;

        auto register_unscoped() -> void;

        auto resume() -> void;

        auto set_events(uint32_t events) -> void;

        auto unlink() noexcept -> void;

        auto update() -> void;
    public:
        system_event() = default;

        system_event(int fd, uint32_t events);

        system_event(const system_event&) = delete;

        system_event(system_event&& other);

        ~system_event();

        auto operator=(const system_event&) -> system_event& = delete;

        auto operator=(system_event&&) -> system_event&;

        auto cancel() -> void;

        auto deregister() -> void;

        auto error(std::exception_ptr ex) noexcept -> void;

        auto latest_events() const noexcept -> uint32_t;

        auto notify() -> void;

        template <typename T>
        auto notify(const T state) -> void {
            awaiters.assign(state);
            notify();
        }

        auto register_scoped() -> register_guard;

        auto wait(uint32_t events, void* state) -> ext::task<detail::awaiter*>;
    };
}
