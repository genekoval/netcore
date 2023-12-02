#include <netcore/async_thread.hpp>
#include <netcore/runtime.hpp>

#include <fmt/std.h>
#include <mutex>
#include <timber/timber>

using unique_lock = std::unique_lock<std::mutex>;

namespace netcore {
    async_thread::async_thread(int max_events, std::string_view name) :
        thread(
            std::bind_front(&async_thread::entry, this),
            max_events,
            std::string(name)
        ) {}

    auto async_thread::alert() -> void {
        const auto lock = unique_lock(mutex);

        // The thread may still be in the process of starting up.
        // It may not have created a valid event handle yet.
        if (event) event.set();
    }

    auto async_thread::entry(
        std::stop_token stoken,
        int max_events,
        std::string name
    ) -> void {
        timber::thread_name = name;

        TIMBER_DEBUG("Thread created with ID: {}", id());

        const auto callback = std::stop_callback(stoken, [this, &name] {
            TIMBER_DEBUG(R"(Requesting to stop thread "{}" ({}))", name, id());
            alert();
        });

        auto rt = runtime(max_events);
        wait_for_tasks(stoken);
        rt.run();
    }

    auto async_thread::id() const noexcept -> std::thread::id {
        return thread.get_id();
    }

    auto async_thread::pop_task() -> std::optional<ext::task<>> {
        const auto lock = unique_lock(mutex);

        if (tasks.empty()) return std::nullopt;

        auto task = std::move(tasks.front());
        tasks.pop();

        return task;
    }

    auto async_thread::push_task(ext::task<>&& task) -> void {
        const auto lock = unique_lock(mutex);
        tasks.emplace(std::forward<ext::task<>>(task));
    }

    auto async_thread::run(ext::task<>&& task) -> void {
        push_task(std::forward<ext::task<>>(task));
        alert();
    }

    auto async_thread::run_task(ext::task<> task, const std::stop_token& stoken)
        -> ext::detached_task {
        ++task_count;

        try {
            TIMBER_DEBUG("Starting task");
            co_await std::move(task);
            TIMBER_DEBUG("Task complete");
        }
        catch (const std::exception& ex) {
            TIMBER_ERROR("error occurred in task: {}", ex.what());
        }
        catch (...) {
            TIMBER_ERROR("unknown error occurred in task");
        }

        --task_count;
        if (stoken.stop_requested() && task_count == 0) event.set();
    }

    auto async_thread::run_tasks(const std::stop_token& stoken) -> void {
        while (true) {
            auto task = pop_task();
            if (!task) break;

            run_task(std::move(*task), stoken);
        }
    }

    auto async_thread::wait_for_tasks(const std::stop_token& stoken)
        -> ext::detached_task {
        auto event = eventfd();

        {
            const auto lock = unique_lock(mutex);
            this->event = event.handle();
        }

        TIMBER_TRACE("Entering task loop");

        while (true) {
            run_tasks(stoken);

            if (stoken.stop_requested()) {
                TIMBER_DEBUG("Thread stop request received");
                break;
            }

            co_await event.wait();
        }

        TIMBER_TRACE("Exiting task loop");

        if (task_count > 0) {
            TIMBER_DEBUG(
                "Waiting for {:L} unfinished task{}",
                task_count,
                task_count == 1 ? "" : "s"
            );

            co_await event.wait();
        }
    }
}
