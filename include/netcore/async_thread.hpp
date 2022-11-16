#pragma once

#include <netcore/eventfd.h>

#include <ext/coroutine>
#include <functional>
#include <queue>
#include <thread>

namespace netcore {
    class async_thread {
        eventfd_handle event;
        std::mutex mutex;
        std::queue<ext::task<>> tasks;
        std::jthread thread;

        auto alert() -> void;

        auto entry(
            std::stop_token stoken,
            int max_events,
            std::string name
        ) -> void;

        auto pop_task() -> std::optional<ext::task<>>;

        auto push_task(ext::task<>&& task) -> void;

        auto run_task(ext::task<>&& task) -> ext::detached_task;

        auto run_tasks() -> void;

        auto wait_for_tasks(
            const std::stop_token& stoken
        ) -> ext::detached_task;
    public:
        async_thread() = default;

        async_thread(int max_events, std::string_view name);

        auto id() const noexcept -> std::thread::id;

        auto run(ext::task<>&& task) -> void;

        auto wait(ext::task<>&& task) -> ext::task<>;

        template <typename T>
        auto wait(ext::task<T>&& task) -> ext::task<T> {
            auto t = std::forward<ext::task<T>>(task);

            co_await wait([](ext::task<T>& task) -> ext::task<> {
                co_await task.when_ready();
            }(t));

            co_return co_await t;
        }
    };
}
