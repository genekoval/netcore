#include <netcore/detail/awaiter.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <utility>

namespace netcore::detail {
    auto awaiter::cancel() noexcept -> void {
        error(std::make_exception_ptr(task_canceled()));
    }

    auto awaiter::error(std::exception_ptr ex) noexcept -> void {
        auto* current = this;

        do {
            current->exception = ex;
            current = current->next;
        } while (current);
    }

    awaiter_queue::awaiter_queue(awaiter_queue&& other) :
        head(std::exchange(other.head, nullptr)),
        tail(std::exchange(other.tail, nullptr)) {}

    auto awaiter_queue::operator=(awaiter_queue&& other) noexcept
        -> awaiter_queue& {
        head = std::exchange(other.head, nullptr);
        tail = std::exchange(other.tail, nullptr);

        return *this;
    }

    auto awaiter_queue::cancel() noexcept -> void {
        if (head) head->cancel();
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

    auto awaiter_queue::error(std::exception_ptr ex) noexcept -> void {
        if (head) head->error(ex);
    }

    auto awaiter_queue::pop() noexcept -> awaiter* {
        if (!head) return nullptr;

        if (head == tail) tail = nullptr;

        auto* a = head;
        head = a->next;
        a->next = nullptr;

        return a;
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

    auto awaiter_queue::size() -> std::size_t {
        std::size_t n = 0;

        auto* current = head;

        while (current) {
            ++n;
            current = current->next;
        }

        return n;
    }

    awaitable::awaitable(awaiter_queue& awaiters, void* state) :
        awaiters(awaiters) {
        a.state = state;
    }

    auto awaitable::await_ready() const noexcept -> bool { return false; }

    auto awaitable::await_suspend(std::coroutine_handle<> coroutine) -> void {
        a.coroutine = coroutine;
        awaiters.enqueue(a);
    }

    auto awaitable::await_resume() -> awaiter* {
        if (a.exception) std::rethrow_exception(a.exception);
        return a.next;
    }
}
