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
        std::chrono::seconds timeout;
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

        auto empty() const noexcept -> bool;

        auto resume_all() -> void;
    public:
        static auto current() -> runtime&;

        const fd descriptor;

        runtime(const runtime_options& options);

        ~runtime();

        auto add(system_event& system_event) -> void;

        auto one_or_empty() const noexcept -> bool;

        auto remove(system_event& system_event) -> void;

        auto run() -> void;

        auto shutting_down() const noexcept -> bool;

        auto shutdown() noexcept -> void;

        auto status() const noexcept -> runtime_status;

        auto stop() noexcept -> void;

        auto update(system_event& system_event) -> void;

        auto enqueue(detail::awaiter& a) -> void;

        auto enqueue(detail::awaiter_queue& awaiters) -> void;
    };
}
