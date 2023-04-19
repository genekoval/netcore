#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <timber/timber>
#include <utility>

namespace {
    constexpr auto permanent_events = EPOLLET;
}

namespace netcore {
    system_event::system_event(
        int fd,
        uint32_t events
    ) :
        fd(fd),
        events(events | permanent_events)
    {}

    system_event::system_event(system_event&& other) :
        fd(std::exchange(other.fd, -1)),
        events(std::exchange(other.events, 0)),
        registered(std::exchange(other.registered, false)),
        awaiters(std::exchange(other.awaiters, {}))
    {}

    auto system_event::operator=(system_event&& other) -> system_event& {
        fd = std::exchange(other.fd, -1);
        events = std::exchange(other.events, 0);
        registered = std::exchange(other.registered, false);
        awaiters = std::exchange(other.awaiters, {});

        return *this;
    }

    auto system_event::cancel() -> void {
        if (registered) runtime::current().remove(this);
        awaiters.cancel();
        notify();
    }

    auto system_event::error(std::exception_ptr ex) noexcept -> void {
        awaiters.error(ex);
        notify();
    }

    auto system_event::latest_events() const noexcept -> uint32_t {
        return events;
    }

    auto system_event::notify() -> void {
        runtime::current().enqueue(awaiters);
    }

    auto system_event::resume() -> void {
        awaiters.resume();
    }

    auto system_event::wait(
        uint32_t events,
        void* state
    ) -> ext::task<detail::awaiter*> {
        class awaitable : public detail::awaitable {
            system_event* event;
        public:
            awaitable(system_event* event, void* state) :
                detail::awaitable(event->awaiters, state),
                event(event)
            {
                if (!event->registered) runtime::current().add(event);
            }

            ~awaitable() {
                if (event->registered) runtime::current().remove(event);
            }

            auto await_suspend(std::coroutine_handle<> coroutine) -> void {
                TIMBER_TRACE("fd ({}) suspended", event->fd);
                detail::awaitable::await_suspend(coroutine);
            }

            auto await_resume() -> detail::awaiter* {
                TIMBER_TRACE("fd ({}) resumed", event->fd);
                return detail::awaitable::await_resume();
            }
        };

        if (events != 0) this->events = events | permanent_events;
        co_return co_await awaitable(this, state);
    }
}
