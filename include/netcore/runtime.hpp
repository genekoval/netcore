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
        class awaiter {
            int descriptor = -1;
            bool canceled = false;
            std::uint32_t submission = 0;
            std::uint32_t completion = 0;
            std::coroutine_handle<> coroutine;
        public:
            awaiter() = default;

            awaiter(int fd) noexcept;

            awaiter(const awaiter&) = delete;

            awaiter(awaiter&& other);

            ~awaiter();

            auto operator=(const awaiter&) -> awaiter& = delete;

            auto operator=(awaiter&& other) -> awaiter&;

            auto add(std::uint32_t events) -> void;

            auto awaiting() const noexcept -> bool;

            auto await_ready() const noexcept -> bool;

            auto await_suspend(std::coroutine_handle<> coroutine) -> void;

            auto await_resume() noexcept -> std::uint32_t;

            auto cancel() -> void;

            auto events() const noexcept -> std::uint32_t;

            auto fd() const noexcept -> int;

            auto resume(std::uint32_t events) -> void;

            auto set(std::uint32_t events) -> void;
        };

        static auto active() -> bool;

        static auto current() -> runtime&;

        const fd descriptor;

        runtime(int max_events = SOMAXCONN);

        ~runtime();

        auto add(awaiter* a) -> void;

        auto run() -> void;

        auto enqueue(detail::awaiter& a) -> void;

        auto enqueue(detail::awaiter_queue& awaiters) -> void;

        auto modify(awaiter* a) -> void;

        auto remove(int fd) -> void;
    };

    auto run() -> void;

    template <typename R>
    auto run(ext::task<R>&& task) -> R {
        auto wrapper = [](ext::task<R>&& task) -> ext::jtask<R> {
            co_return co_await std::move(task);
        };

        auto wrapper_task = ext::jtask<>();

        if (runtime::active()) {
            wrapper_task = wrapper(std::forward<ext::task<R>>(task));
            runtime::current().run();
        }
        else {
            auto runtime = netcore::runtime();
            wrapper_task = wrapper(std::forward<ext::task<R>>(task));
            runtime.run();
        }

        return std::move(wrapper_task).result();
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
