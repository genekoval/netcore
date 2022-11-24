#include <netcore/event.hpp>

namespace netcore {
    auto event<void>::emit() -> void {
        detail::event::emit();
    }

    auto event<void>::listen() -> ext::task<> {
        co_await detail::awaitable(listeners, nullptr);
    }
}

namespace netcore::detail {
    auto event::cancel() -> void {
        listeners.cancel();
        emit();
    }

    auto event::emit() -> void {
        detail::notifier::instance().enqueue(listeners);
    }
}
