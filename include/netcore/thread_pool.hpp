#pragma once

#include "eventfd.h"

#include <condition_variable>
#include <ext/coroutine>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

namespace netcore {
    class thread_pool {
        using job_type = std::function<void()>;

        std::queue<job_type> jobs;
        std::vector<std::jthread> threads;
        std::condition_variable condition;
        std::mutex mutex;

        auto entry(std::stop_token stoken) -> void;
    public:
        thread_pool(
            std::string_view name,
            std::size_t size,
            std::size_t backlog_scale_factor = 1
        );

        ~thread_pool();

        auto run(job_type&& job) -> void;

        template <typename F>
        requires std::same_as<void, std::invoke_result_t<F>>
        auto wait(F&& f) -> ext::jtask<> {
            auto event = eventfd();
            auto exception = std::exception_ptr();

            run([
                f = std::forward<F>(f),
                handle = event.handle(),
                &exception
            ]() mutable {
                try {
                    f();
                }
                catch (...) {
                    exception = std::current_exception();
                }

                handle.set();
            });

            co_await event.wait();
            if (exception) std::rethrow_exception(exception);
        }

        template <typename F, typename R = std::invoke_result_t<F>>
        requires std::move_constructible<R>
        auto wait(F&& f) -> ext::jtask<R> {
            alignas(R) std::byte storage[sizeof(R)];

            co_await wait([f = std::forward<F>(f), &storage]() -> void {
                new (static_cast<void*>(storage)) R(f());
            });

            co_return std::move(*std::launder(reinterpret_cast<R*>(&storage)));
        }
    };
}
