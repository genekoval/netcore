#pragma once

#include <netcore/async_thread.hpp>

namespace netcore {
    class thread_pool {
        std::size_t current = 0;
        std::vector<std::unique_ptr<async_thread>> threads;

        auto thread() noexcept -> async_thread&;
    public:
        thread_pool(int count, unsigned int max_events, std::string_view name);

        auto run(ext::task<>&& task) -> void;

        auto wait(ext::task<>&& task) -> ext::task<>;

        template <typename T>
        auto wait(ext::task<T>&& task) -> ext::task<T> {
            co_return co_await thread().wait(std::forward<ext::task<T>>(task));
        }
    };
}
