#pragma once

#include <coroutine>

namespace netcore::detail {
    struct awaiter final {
        std::coroutine_handle<> coroutine = nullptr;
        awaiter* next = nullptr;
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

        auto empty() const noexcept -> bool;

        auto enqueue(awaiter& a) -> void;

        auto enqueue(awaiter_queue& other) -> void;

        auto resume() -> void;
    };

    class awaitable final {
        awaiter a;
        awaiter_queue& awaiters;

        auto check_runtime_status() const -> void;
    public:
        awaitable(awaiter_queue& awaiters);

        auto await_ready() const noexcept -> bool;

        auto await_suspend(std::coroutine_handle<> coroutine) -> void;

        auto await_resume() -> void;
    };
}
