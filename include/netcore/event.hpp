#pragma once

#include "detail/awaiter.hpp"

#include <ext/coroutine>

namespace netcore {
    namespace detail {
        class event {
        protected:
            detail::awaiter_queue listeners;

            auto emit() -> void;
        public:
            auto cancel() -> void;
        };
    }

    template <typename T = void>
    requires std::default_initializable<T> || std::same_as<T, void>
    struct event : detail::event {
        auto emit(const T& value) -> void {
            listeners.assign(value);
            detail::event::emit();
        }

        auto listen() -> ext::task<T> {
            auto value = T();

            co_await detail::awaitable(listeners, &value);

            co_return value;
        }
    };

    template <>
    struct event<void> : detail::event {
        auto emit() -> void;

        auto listen() -> ext::task<>;
    };
}
