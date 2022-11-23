#pragma once

#include <netcore/detail/awaiter.hpp>
#include <netcore/detail/notifier.hpp>

#include <ext/coroutine>

namespace netcore {
    class event {
        detail::awaiter_queue listeners;
    public:
        auto cancel() -> void;

        auto emit() -> void;

        auto listen() -> ext::task<>;
    };
}
