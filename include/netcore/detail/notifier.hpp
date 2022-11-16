#pragma once

#include "awaiter.hpp"

#include <netcore/fd.h>

#include <chrono>
#include <ext/coroutine>
#include <memory>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace netcore::detail {
    class notification;
}

namespace netcore {
    class [[nodiscard]] register_guard {
        friend class detail::notification;

        detail::notification* notif;

        register_guard(detail::notification* notif);
    public:
        ~register_guard();
    };
}

namespace netcore::detail {
    class notifier;

    class notification {
        friend class connect_scope;
        friend class disconnect_scope;
        friend class notifier;

        int fd = -1;
        uint32_t events = 0;
        notification* head = this;
        notification* tail = this;
        awaiter_queue awaiters;
        notifier* last_notifier = nullptr;

        auto append(notification& other) noexcept -> void;

        auto empty() const noexcept -> bool;

        auto one_or_empty() const noexcept -> bool;

        auto register_unscoped() -> void;

        auto resume() -> void;

        auto set_events(uint32_t events) -> void;

        auto unlink() noexcept -> void;

        auto update() -> void;
    public:
        notification() = default;

        notification(int fd, uint32_t events);

        notification(const notification&) = delete;

        notification(notification&& other);

        ~notification();

        auto operator=(const notification&) -> notification& = delete;

        auto operator=(notification&&) -> notification&;

        auto register_scoped() -> register_guard;

        auto deregister() -> void;

        auto wait(uint32_t events = 0) -> ext::task<>;
    };

    struct notifier_options {
        int max_events = SOMAXCONN;
        std::chrono::seconds timeout;
    };

    enum class notifier_status : uint8_t {
        stopped,
        running,
        graceful_shutdown,
        force_shutdown
    };

    class notifier {
        const std::unique_ptr<epoll_event[]> events;
        const int max_events;
        const std::chrono::milliseconds timeout;

        notification notifications;
        awaiter_queue pending;
        notifier_status stat = notifier_status::stopped;

        auto empty() const noexcept -> bool;

        auto resume_all() -> void;
    public:
        static auto instance() -> notifier&;

        const fd descriptor;

        notifier(const notifier_options& options);

        ~notifier();

        auto add(notification& notification) -> void;

        auto one_or_empty() const noexcept -> bool;

        auto remove(notification& notification) -> void;

        auto run() -> void;

        auto shutting_down() const noexcept -> bool;

        auto shutdown() noexcept -> void;

        auto status() const noexcept -> notifier_status;

        auto stop() noexcept -> void;

        auto update(notification& notification) -> void;

        auto enqueue(awaiter& a) -> void;

        auto enqueue(awaiter_queue& awaiters) -> void;
    };
}
