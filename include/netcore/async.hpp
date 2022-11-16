#pragma once

#include <chrono>
#include <ext/coroutine>
#include <span>

namespace netcore {
    struct async_options {
        int max_events;
        std::chrono::seconds timeout;
        std::span<const int> signals;
    };

    auto async(ext::task<>&& task) -> void;

    auto async(const async_options& options, ext::task<>&& task) -> void;

    auto yield() -> ext::task<>;
}
