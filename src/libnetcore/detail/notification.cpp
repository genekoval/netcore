#include "netcore/except.hpp"
#include <netcore/detail/notifier.hpp>

#include <timber/timber>

namespace {
    constexpr auto permanent_events = EPOLLET;
}

namespace netcore {
    register_guard::register_guard(detail::notification* notif) :
        notif(notif)
    {}

    register_guard::~register_guard() {
        notif->deregister();
    }
}

namespace netcore::detail {
    notification::notification(
        int fd,
        uint32_t events
    ) :
        fd(fd),
        events(events | permanent_events)
    {
        register_unscoped();
    }

    notification::notification(notification&& other) :
        fd(std::exchange(other.fd, -1)),
        events(std::exchange(other.events, 0)),
        head(std::exchange(other.head, &other)),
        tail(std::exchange(other.tail, &other)),
        awaiters(std::exchange(other.awaiters, {})),
        last_notifier(std::exchange(other.last_notifier, nullptr))
    {
        head->tail = this;
        tail->head = this;

        update();
    }

    notification::~notification() {
        unlink();
    }

    auto notification::operator=(notification&& other) -> notification& {
        fd = std::exchange(other.fd, -1);
        events = std::exchange(other.events, 0);
        head = std::exchange(other.head, &other);
        tail = std::exchange(other.tail, &other);
        awaiters = std::exchange(other.awaiters, {});
        last_notifier = std::exchange(other.last_notifier, nullptr);

        head->tail = this;
        tail->head = this;

        update();

        return *this;
    }

    auto notification::append(notification& other) noexcept -> void {
        other.head = this;
        other.tail = tail;

        tail->head = &other;
        tail = &other;
    }

    auto notification::register_scoped() -> register_guard {
        register_unscoped();
        return register_guard(this);
    }

    auto notification::register_unscoped() -> void {
        auto* const instance = &notifier::instance();
        instance->add(*this);
        last_notifier = instance;
    }

    auto notification::deregister() -> void {
        unlink();
        notifier::instance().remove(*this);
        last_notifier = nullptr;
    }

    auto notification::empty() const noexcept -> bool {
        return head == this && tail == this;
    }

    auto notification::one_or_empty() const noexcept -> bool {
        return head == tail;
    }

    auto notification::resume() -> void {
        awaiters.resume();
    }

    auto notification::set_events(uint32_t events) -> void {
        auto update_required = false;

        if (events != 0 && events != this->events) {
            this->events = events | permanent_events;
            update_required = true;
        }

        if (!last_notifier) {
            register_unscoped();
            return;
        }

        if (last_notifier == &notifier::instance()) {
            if (update_required) update();
            return;
        }

        throw std::runtime_error(
            "attempt to use one runtime while registered to another"
        );
    }

    auto notification::unlink() noexcept -> void {
        head->tail = tail;
        tail->head = head;

        head = this;
        tail = this;
    }

    auto notification::update() -> void {
        notifier::instance().update(*this);
    }

    auto notification::wait(uint32_t events) -> ext::task<> {
        set_events(events);

        TIMBER_TRACE("fd ({}) suspended", fd);

        try {
            co_await awaitable(awaiters);
            TIMBER_TRACE("fd ({}) resumed", fd);
        }
        catch (const task_canceled&) {
            TIMBER_DEBUG("fd ({}) canceled", fd);
            throw;
        }
    }
}
