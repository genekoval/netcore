#pragma once

#include <coroutine>
#include <exception>

namespace netcore::detail {
    struct awaiter final {
        std::coroutine_handle<> coroutine = nullptr;
        awaiter* next = nullptr;
        void* state = nullptr;
        std::exception_ptr exception;

        template <typename T>
        auto assign(const T state) const noexcept -> void {
            auto* current = this;

            do {
                auto& value = *static_cast<T*>(current->state);
                value = state;

                current = current->next;
            } while (current);
        }

        auto cancel() noexcept -> void;

        auto error(std::exception_ptr ex) noexcept -> void;
    };

    class awaiter_queue final {
        awaiter* head = nullptr;
        awaiter* tail = nullptr;
    public:
        awaiter_queue() = default;

        awaiter_queue(const awaiter_queue&) = delete;

        awaiter_queue(awaiter_queue&& other);

        auto operator=(const awaiter_queue&) -> awaiter_queue& = delete;

        auto operator=(awaiter_queue&& other) noexcept -> awaiter_queue&;

        template <typename T>
        auto assign(const T state) const noexcept -> void {
            if (head) head->assign(state);
        }

        auto cancel() noexcept -> void;

        auto empty() const noexcept -> bool;

        auto enqueue(awaiter& a) -> void;

        auto enqueue(awaiter_queue& other) -> void;

        auto error(std::exception_ptr ex) noexcept -> void;

        auto resume() -> void;
    };

    class awaitable final {
        awaiter a;
        awaiter_queue& awaiters;
    public:
        awaitable(awaiter_queue& awaiters, void* state);

        auto await_ready() const noexcept -> bool;

        auto await_suspend(std::coroutine_handle<> coroutine) -> void;

        auto await_resume() -> awaiter*;
    };
}
