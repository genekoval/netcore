#pragma once

#include <netcore/eventfd.hpp>

#include <ext/coroutine>
#include <functional>
#include <optional>
#include <queue>
#include <thread>

namespace netcore {
    class async_thread {
        eventfd_handle event;
        std::mutex mutex;
        std::queue<ext::task<>> tasks;
        std::jthread thread;
        std::size_t task_count = 0;

        auto alert() -> void;

        auto entry(
            std::stop_token stoken,
            int max_events,
            std::string name
        ) -> void;

        auto pop_task() -> std::optional<ext::task<>>;

        auto push_task(ext::task<>&& task) -> void;

        auto run_task(
            ext::task<> task,
            const std::stop_token& stoken
        ) -> ext::detached_task;

        auto run_tasks(const std::stop_token& stoken) -> void;

        auto wait_for_tasks(
            const std::stop_token& stoken
        ) -> ext::detached_task;
    public:
        async_thread() = default;

        async_thread(int max_events, std::string_view name);

        auto id() const noexcept -> std::thread::id;

        auto run(ext::task<>&& task) -> void;

        template <typename T>
        auto wait(ext::task<T>&& task) -> ext::task<T> {
            auto complete = eventfd();

            run([](
                const ext::task<T>& task,
                eventfd_handle complete
            ) -> ext::task<> {
                co_await task.when_ready();
                complete.set();
            }(task, complete.handle()));

            co_await complete.wait();
            co_return co_await std::move(task);
        }
    };
}
