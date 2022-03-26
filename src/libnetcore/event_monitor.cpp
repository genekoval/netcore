#include <netcore/event_monitor.h>

#include <ext/except.h>
#include <timber/timber>
#include <sys/epoll.h>

namespace netcore {
    constexpr auto epoll_wait_timeout = -1; // Infinite

    event_monitor::event_monitor(int max_events) :
        descriptor(epoll_create1(0)),
        events(std::make_unique<epoll_event[]>(max_events)),
        max_events(max_events)
    {
        if (!descriptor.valid()) {
            throw ext::system_error("epoll create failure");
        }

        TIMBER_DEBUG(
            "event monitor ({}) created with {} max events",
            descriptor,
            max_events
        );
    }

    auto event_monitor::add(int fd) -> void {
        ev.data.fd = fd;
        if (epoll_ctl(descriptor, EPOLL_CTL_ADD, fd, &ev) == -1) {
            throw ext::system_error(
                "Event monitor failed to add socket: " + fd
            );
        }
    }

    auto event_monitor::set(uint32_t event_types) -> void {
        ev.events = event_types;
    }

    auto event_monitor::wait(const event_handler& callback) const -> void {
        const auto ready = epoll_wait(
            descriptor,
            events.get(),
            max_events,
            epoll_wait_timeout
        );

        if (ready == -1) {
            throw ext::system_error("epoll wait failure");
        }

        for (auto i = 0; i < ready; i++) callback(events[i]);
    }
}
