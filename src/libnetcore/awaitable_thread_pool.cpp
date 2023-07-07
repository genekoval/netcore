#include <netcore/awaitable_thread_pool.hpp>

#include <timber/timber>

using lock = std::unique_lock<std::mutex>;

namespace netcore {
    awaitable_thread_pool::awaitable_thread_pool(
        std::string_view name,
        std::size_t size,
        std::size_t backlog_scale_factor
    ) :
        pool(name, size, backlog_scale_factor),
        task(wait_for_events())
    {}

    auto awaitable_thread_pool::enqueue(node& node) -> void {
        {
            const auto lk = lock(mutex);

            if (!head) head = &node;
            if (tail) tail->next = &node;
            tail = &node;
        }

        event.handle().set();
    }

    auto awaitable_thread_pool::resume() -> void {
        auto* current = take_nodes();

        while (current) {
            const auto coroutine = current->coroutine;
            current = current->next;

            coroutine.resume();
        }
    }

    auto awaitable_thread_pool::run(std::function<void()>&& job) -> void {
        pool.run(std::move(job));
    }

    auto awaitable_thread_pool::wait_for_events() -> ext::jtask<> {
        while (true) {
            co_await event.wait();
            resume();
        }
    }

    auto awaitable_thread_pool::take_nodes() -> node* {
        const auto lk = lock(mutex);

        node* result = head;

        head = nullptr;
        tail = nullptr;

        return result;
    }
}
