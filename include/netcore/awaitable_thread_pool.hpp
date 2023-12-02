#pragma once

#include "eventfd.hpp"
#include "thread_pool.hpp"

namespace netcore {
    class awaitable_thread_pool {
        struct node {
            std::coroutine_handle<> coroutine;
            node* next = nullptr;
        };

        std::mutex mutex;
        eventfd event;
        node* head = nullptr;
        node* tail = nullptr;
        thread_pool pool;
        ext::jtask<> task;

        auto enqueue(node& node) -> void;

        auto resume() -> void;

        auto take_nodes() -> node*;

        auto wait_for_events() -> ext::jtask<>;
    public:
        awaitable_thread_pool(
            std::string_view name,
            std::size_t size,
            std::size_t backlog_scale_factor = 1
        );

        template <typename F>
        requires std::same_as<std::invoke_result_t<F>, void>
        auto await(F&& f) -> ext::jtask<> {
            class awaitable {
                F f;
                awaitable_thread_pool& pool;
                std::exception_ptr exception;
                awaitable_thread_pool::node node;
            public:
                awaitable(F&& f, awaitable_thread_pool& pool) :
                    f(std::forward<F>(f)),
                    pool(pool) {}

                auto await_ready() const noexcept -> bool { return false; }

                auto await_suspend(std::coroutine_handle<> coroutine) noexcept
                    -> void {
                    node.coroutine = coroutine;

                    pool.run([this]() -> void {
                        try {
                            f();
                        }
                        catch (...) {
                            exception = std::current_exception();
                        }

                        pool.enqueue(node);
                    });
                }

                auto await_resume() const -> void {
                    if (exception) std::rethrow_exception(exception);
                }
            };

            co_await awaitable(std::forward<F>(f), *this);
        }

        template <typename F, typename R = std::invoke_result_t<F>>
        requires std::movable<R> &&
                 std::convertible_to<std::invoke_result_t<F>, R>
        auto await(F&& f) -> ext::jtask<R> {
            auto result = std::optional<R>();

            co_await await([&result, f = std::move(f)]() -> void {
                result = f();
            });

            co_return std::move(*result);
        }

        auto run(std::function<void()>&& job) -> void;
    };
}
