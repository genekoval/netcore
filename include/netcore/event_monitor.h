#pragma once

#include <netcore/socket.h>

#include <functional>
#include <memory>
#include <sys/epoll.h>

namespace netcore {
    using event_handler = std::function<void (const epoll_event&)>;

    class event_monitor {
        const socket epsock;
        epoll_event ev;
        std::unique_ptr<epoll_event[]> events;
        int max_events;
    public:
        event_monitor(int max_events);

        auto add(int fd) -> void;
        auto add(const socket& sock) -> void;

        auto set(uint32_t event_types) -> void;

        auto wait(const event_handler& callback) -> void;
    };
}
