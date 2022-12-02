#pragma once

#include "fd.h"
#include "system_event.hpp"

#include <chrono>
#include <memory>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace netcore {
    struct runtime_options {
        int max_events = SOMAXCONN;
        std::chrono::seconds timeout = std::chrono::seconds::zero();
    };

    enum class runtime_status : uint8_t {
        stopped,
        running,
        graceful_shutdown,
        force_shutdown
    };

    class runtime {
        const std::unique_ptr<epoll_event[]> events;
        const int max_events;
        const std::chrono::milliseconds timeout;

        system_event system_events;
        detail::awaiter_queue pending;
        runtime_status stat = runtime_status::stopped;

        auto cancel() -> void;

        auto notify() -> void;

        auto resume_all() -> void;

        /**
         * Perform either a normal or EOL (end of life) run.
         */
        auto run(bool eol) -> void;

        auto run_main_task(
            ext::task<>&& task,
            std::exception_ptr& exception
        ) -> ext::detached_task;
    public:
        static auto current() -> runtime&;

        const fd descriptor;

        /**
         * Creates a runtime with the following default options:
         * - Max Events: SOMAXCONN
         * - Timeout: 0 seconds
         */
        runtime();

        runtime(const runtime_options& options);

        ~runtime();

        auto add(system_event& system_event) -> void;

        auto one_or_empty() const noexcept -> bool;

        auto remove(system_event& system_event) -> void;

        auto run() -> void;

        auto run(ext::task<>&& task) -> void;

        auto shutting_down() const noexcept -> bool;

        auto shutdown() noexcept -> void;

        auto status() const noexcept -> runtime_status;

        auto stop() noexcept -> void;

        auto update(system_event& system_event) -> void;

        auto enqueue(detail::awaiter& a) -> void;

        auto enqueue(detail::awaiter_queue& awaiters) -> void;
    };
}
