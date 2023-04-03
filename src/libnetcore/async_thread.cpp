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
        )
    {}

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

        auto rt = runtime(runtime_options {
            .max_events = max_events
        });

        wait_for_tasks(stoken);

        rt.run();
    }

    auto async_thread::id() const noexcept -> std::thread::id {
        return thread.get_id();
    }

    auto async_thread::pop_task() -> std::optional<ext::task<>> {
        const auto lock = unique_lock(mutex);

        if (tasks.empty()) return {};

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

    auto async_thread::run_task(ext::task<>&& task) -> ext::detached_task {
        const auto t = std::forward<ext::task<>>(task);

        try {
            co_await t;
        }
        catch (const std::exception& ex) {
            TIMBER_ERROR("error occurred in task: {}", ex.what());
        }
        catch (...) {
            TIMBER_ERROR("unknown error occurred in task");
        }
    }

    auto async_thread::run_tasks() -> void {
        while (true) {
            auto task = pop_task();
            if (!task) break;

            run_task(std::move(*task));
        }
    }

    auto async_thread::wait(ext::task<>&& task) -> ext::task<> {
        auto t = std::forward<ext::task<>>(task);

        auto complete = eventfd();
        auto handle = complete.handle();

        auto exception = std::exception_ptr();

        run([](
            ext::task<>& task,
            eventfd_handle& handle,
            std::exception_ptr& exception
        ) -> ext::task<> {
            try {
                co_await task;
            }
            catch (...) {
                exception = std::current_exception();
            }

            handle.set();
        }(t, handle, exception));

        co_await complete.wait();

        if (exception) std::rethrow_exception(exception);
    }

    auto async_thread::wait_for_tasks(
        const std::stop_token& stoken
    ) -> ext::detached_task {
        auto event = eventfd();

        {
            const auto lock = unique_lock(mutex);
            this->event = event.handle();
        }

        while (true) {
            run_tasks();

            if (stoken.stop_requested()) {
                TIMBER_DEBUG("Thread stop request received");
                runtime::current().stop();
                break;
            }

            co_await event.wait();
        }

        TIMBER_TRACE("Exiting task loop");
    }
}
