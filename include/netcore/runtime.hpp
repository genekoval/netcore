#pragma once

#include "detail/awaiter.hpp"
#include "fd.hpp"

#include <chrono>
#include <ext/coroutine>
#include <ext/except.h>
#include <memory>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <timber/timber>

namespace netcore {
    class runtime {
        const std::unique_ptr<epoll_event[]> events;
        const int max_events;

        detail::awaiter_queue pending;
        unsigned long awaiters = 0;
    public:
        class event : public std::enable_shared_from_this<event> {
            int descriptor;
            std::uint32_t received;
            bool canceled = false;

            event(int fd, std::uint32_t events) noexcept;
        public:
            class awaitable {
                runtime::event& event;
                std::coroutine_handle<>& coroutine;
            public:
                awaitable(
                    runtime::event& event,
                    std::coroutine_handle<>& coroutine
                ) noexcept;

                ~awaitable();

                auto await_ready() const noexcept -> bool;

                auto await_suspend(std::coroutine_handle<> coroutine) -> void;

                [[nodiscard]]
                auto await_resume() noexcept -> std::uint32_t;
            };

            static auto create(
                int fd,
                std::uint32_t events
            ) noexcept -> std::shared_ptr<event>;

            std::uint32_t events;
            std::coroutine_handle<> awaiting_in;
            std::coroutine_handle<> awaiting_out;

            event(const event&) = delete;

            event(event&& other) = delete;

            auto operator=(const event&) -> event& = delete;

            auto operator=(event&& other) -> event& = delete;

            auto cancel() -> void;

            auto fd() const noexcept -> int;

            auto in() noexcept -> awaitable;

            auto out() noexcept -> awaitable;

            auto remove() const noexcept -> std::error_code;

            auto resume(std::uint32_t events) -> void;
        };

        friend class event::awaitable;

        static auto active() -> bool;

        static auto current() -> runtime&;

        const fd descriptor;

        runtime(int max_events = SOMAXCONN);

        ~runtime();

        auto add(runtime::event* event) -> void;

        auto run() -> void;

        auto enqueue(detail::awaiter& a) -> void;

        auto enqueue(detail::awaiter_queue& awaiters) -> void;

        auto modify(runtime::event* event) -> void;

        auto remove(int fd) const noexcept -> std::error_code;
    };

    auto run() -> void;

    template <typename R>
    auto run(ext::task<R>&& task) -> R {
        const auto start = [&task]() -> ext::jtask<R> {
            co_return co_await std::move(task);
        };

        auto wrapper = ext::jtask<R>();

        if (runtime::active()) {
            wrapper = start();
            runtime::current().run();
        }
        else {
            auto runtime = netcore::runtime();
            wrapper = start();
            runtime.run();
        }

        return std::move(wrapper).result();
    }

    auto yield() -> ext::task<>;
}

template <>
struct fmt::formatter<netcore::runtime> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }

    auto format(const netcore::runtime& runtime, auto& ctx) {
        return format_to(ctx.out(), "runtime ({})", runtime.descriptor);
    }
};
