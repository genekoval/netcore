#pragma once

#include <netcore/fd.h>

#include <functional>
#include <memory>
#include <sys/epoll.h>

namespace netcore {
    using event_handler = std::function<void(const epoll_event&)>;

    class event_monitor {
        const fd descriptor;
        epoll_event ev;
        const std::unique_ptr<epoll_event[]> events;
        const int max_events;
    public:
        event_monitor(int max_events);

        auto add(int fd) -> void;

        auto set(uint32_t event_types) -> void;

        auto wait(const event_handler& callback) const -> void;
    };
}
