#include <netcore/async_thread_pool.hpp>

#include <fmt/core.h>

namespace netcore {
    async_thread_pool::async_thread_pool(
        int count,
        unsigned int max_events,
        std::string_view name
    ) {
        threads.reserve(count);

        for (auto i = 1; i <= count; ++i) {
            threads.emplace_back(new async_thread(
                max_events,
                fmt::format("{}[{}]", name, i)
            ));
        }
    }

    auto async_thread_pool::run(ext::task<>&& task) -> void {
        thread().run(std::forward<ext::task<>>(task));
    }

    auto async_thread_pool::thread() noexcept -> async_thread& {
        auto& result = *threads[current];
        current = (current + 1) % threads.size();
        return result;
    }

    auto async_thread_pool::wait(ext::task<>&& task) -> ext::task<> {
        co_await thread().wait(std::forward<ext::task<>>(task));
    }
}
