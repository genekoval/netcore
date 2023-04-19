#pragma once

#include "fd.h"
#include "system_event.hpp"

#include <chrono>
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
        std::size_t system_events = 0;
    public:
        static auto active() -> bool;

        static auto current() -> runtime&;

        const fd descriptor;

        runtime(int max_events = SOMAXCONN);

        ~runtime();

        auto add(system_event* system_event) -> void;

        auto run() -> void;

        auto enqueue(detail::awaiter& a) -> void;

        auto enqueue(detail::awaiter_queue& awaiters) -> void;

        auto remove(system_event* system_event) -> void;
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
