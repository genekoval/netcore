#include <netcore/event.hpp>

namespace netcore {
    auto event::cancel() -> void {
        listeners.cancel();
        emit();
    }

    auto event::emit() -> void {
        detail::notifier::instance().enqueue(listeners);
    }

    auto event::listen() -> ext::task<> {
        co_await detail::awaitable(listeners, nullptr);
    }
}
