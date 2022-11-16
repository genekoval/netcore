#include <netcore/event.hpp>

namespace netcore {
    auto event::emit() -> void {
        detail::notifier::instance().enqueue(listeners);
    }

    auto event::listen() -> ext::task<> {
        co_await detail::awaitable(listeners);
    }
}
