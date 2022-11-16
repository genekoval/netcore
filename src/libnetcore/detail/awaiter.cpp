#include <netcore/detail/awaiter.hpp>
#include <netcore/detail/notifier.hpp>
#include <netcore/except.hpp>

#include <utility>

namespace netcore::detail {
    awaiter_queue::awaiter_queue(awaiter_queue&& other) :
        head(std::exchange(other.head, nullptr)),
        tail(std::exchange(other.tail, nullptr))
    {}

    auto awaiter_queue::operator=(
        awaiter_queue&& other
    ) noexcept -> awaiter_queue& {
        head = std::exchange(other.head, nullptr);
        tail = std::exchange(other.tail, nullptr);

        return *this;
    }

    auto awaiter_queue::empty() const noexcept -> bool {
        return head == nullptr;
    }

    auto awaiter_queue::enqueue(awaiter& a) -> void {
        if (!head) head = &a;

        if (tail) tail->next = &a;

        tail = &a;
    }

    auto awaiter_queue::enqueue(awaiter_queue& other) -> void {
        if (other.empty()) return;

        enqueue(*other.head);
        tail = other.tail;

        other.head = nullptr;
        other.tail = nullptr;
    }

    auto awaiter_queue::resume() -> void {
        // Make a local copy of the awaiter list, and create a new list
        // as resumed coroutines could add more awaiters.
        auto* current = std::exchange(head, nullptr);
        tail = nullptr;

        while (current) {
            // After the coroutine 'resume' call,
            // the object pointed to by 'current' will cease to exist.
            const auto coroutine = current->coroutine;
            current = current->next;

            if (coroutine && !coroutine.done()) coroutine.resume();
        }
    }

    awaitable::awaitable(awaiter_queue& awaiters) :
        awaiters(awaiters)
    {}

    auto awaitable::await_ready() const noexcept -> bool {
        return false;
    }

    auto awaitable::check_runtime_status() const -> void {
        const auto status = notifier::instance().status();

        if (status == notifier_status::force_shutdown) {
            throw task_canceled();
        }
    }

    auto awaitable::await_suspend(std::coroutine_handle<> coroutine) -> void {
        check_runtime_status();

        a.coroutine = coroutine;
        awaiters.enqueue(a);
    }

    auto awaitable::await_resume() -> void {
        check_runtime_status();
    }
}
