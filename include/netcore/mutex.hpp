#pragma once

#include "detail/awaiter.hpp"
#include "runtime.hpp"

#include <ext/coroutine>
#include <utility>

namespace netcore {
    template <typename T>
    class mutex {
        detail::awaiter_queue awaiters;
        T data;
        bool locked = false;

        auto unlock() -> void {
            if (auto* next = awaiters.pop()) {
                runtime::current().enqueue(*next);
            }
            else locked = false;
        }
    public:
        class guard {
            friend class mutex;

            mutex* mut = nullptr;

            guard(mutex* mut) : mut(mut) {}
        public:
            guard() = default;

            guard(const guard&) = delete;

            guard(guard&& other) : mut(std::exchange(other.mut, nullptr)) {}

            ~guard() {
                if (mut) mut->unlock();
            }

            auto operator=(const guard&) -> guard& = delete;

            auto operator=(guard&& other) -> guard& {
                mut = std::exchange(other.mut, nullptr);
                return *this;
            }

            auto operator*() const noexcept -> T& { return mut->data; }

            auto operator->() const noexcept -> T* { return &mut->data; }
        };

        friend class guard;

        template <typename... Args>
        mutex(Args&&... args) : data(std::forward<Args>(args)...) {}

        mutex(const mutex&) = delete;

        mutex(mutex&&) = delete;

        auto operator=(const mutex&) -> mutex& = delete;

        auto operator=(mutex&&) -> mutex& = delete;

        auto get() noexcept -> T& { return data; }

        auto lock() -> ext::task<guard> {
            if (locked) co_await detail::awaitable(awaiters, nullptr);

            locked = true;
            co_return guard(this);
        }
    };
}
