#include <netcore/except.hpp>
#include <netcore/system_event.hpp>

#include <timber/timber>
#include <utility>

namespace netcore {
    system_event::system_event(int fd) : awaiter(fd) {}

    auto system_event::add(std::uint32_t events) -> void {
        awaiter.add(events);
    }

    auto system_event::awaiting() const noexcept -> bool {
        return awaiter.awaiting();
    }

    auto system_event::cancel() -> void {
        awaiter.cancel();
    }

    auto system_event::in() -> ext::task<bool> {
        co_return co_await wait_for(EPOLLIN);
    }

    auto system_event::out() -> ext::task<bool> {
        co_return co_await wait_for(EPOLLOUT);
    }

    auto system_event::set(std::uint32_t events) -> void {
        awaiter.set(events);
    }

    auto system_event::wait() -> ext::task<std::uint32_t> {
        co_return co_await awaiter;
    }

    auto system_event::wait_for(
        std::uint32_t events
    ) -> ext::task<std::uint32_t> {
        set(events);
        return wait();
    }
}
