#include <netcore/bidirectional_event.hpp>
#include <netcore/except.hpp>

namespace netcore {
    bidirectional_event::bidirectional_event(int fd) : inner(fd) {}

    auto bidirectional_event::cancel() -> void {
        inner.cancel();
    }

    auto bidirectional_event::in() -> ext::task<> {
        return wait_for(EPOLLIN);
    }

    auto bidirectional_event::out() -> ext::task<> {
        return wait_for(EPOLLOUT);
    }

    auto bidirectional_event::wait() -> ext::task<std::uint32_t> {
        try {
            const auto events = co_await inner.wait();
            if (events == 0) throw task_canceled();
            co_return events;
        }
        catch (...) {
            if (continuation) continuation.resume(std::current_exception());
            throw;
        }
    }

    auto bidirectional_event::wait_for(std::uint32_t event) -> ext::task<> {
        inner.add(event);

        if (inner.awaiting()) {
            const auto events = co_await continuation;
            if (event & events) co_return;

            inner.set(event);
        }

        while (true) {
            const auto events = co_await wait();

            if (continuation) continuation.resume(events);
            if (event & events) co_return;

            inner.set(event);
        }
    }
}
