#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <timber/timber>

namespace {
    constexpr auto permanent_events = EPOLLET;
}

namespace netcore {
    register_guard::register_guard(system_event& notif) : notif(notif) {}

    register_guard::~register_guard() {
        notif.deregister();
    }

    system_event::system_event(
        int fd,
        uint32_t events
    ) :
        fd(fd),
        events(events | permanent_events)
    {
        register_unscoped();
    }

    system_event::system_event(system_event&& other) :
        fd(std::exchange(other.fd, -1)),
        events(std::exchange(other.events, 0)),
        head(std::exchange(other.head, &other)),
        tail(std::exchange(other.tail, &other)),
        awaiters(std::exchange(other.awaiters, {})),
        last_runtime(std::exchange(other.last_runtime, nullptr))
    {
        head->tail = this;
        tail->head = this;

        update();
    }

    system_event::~system_event() {
        unlink();
    }

    auto system_event::operator=(system_event&& other) -> system_event& {
        fd = std::exchange(other.fd, -1);
        events = std::exchange(other.events, 0);
        head = std::exchange(other.head, &other);
        tail = std::exchange(other.tail, &other);
        awaiters = std::exchange(other.awaiters, {});
        last_runtime = std::exchange(other.last_runtime, nullptr);

        head->tail = this;
        tail->head = this;

        update();

        return *this;
    }

    auto system_event::append(system_event& other) noexcept -> void {
        other.head = this;
        other.tail = tail;

        tail->head = &other;
        tail = &other;
    }

    auto system_event::cancel() -> void {
        awaiters.cancel();
        notify();
    }

    auto system_event::register_scoped() -> register_guard {
        register_unscoped();
        return register_guard(*this);
    }

    auto system_event::register_unscoped() -> void {
        auto* const current = &runtime::current();
        current->add(*this);
        last_runtime = current;
    }

    auto system_event::deregister() -> void {
        unlink();
        runtime::current().remove(*this);
        last_runtime = nullptr;
    }

    auto system_event::empty() const noexcept -> bool {
        return head == this && tail == this;
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

    auto system_event::one_or_empty() const noexcept -> bool {
        return head == tail;
    }

    auto system_event::resume() -> void {
        awaiters.resume();
    }

    auto system_event::set_events(uint32_t events) -> void {
        auto update_required = false;

        if (events != 0 && events != this->events) {
            this->events = events | permanent_events;
            update_required = true;
        }

        if (!last_runtime) {
            register_unscoped();
            return;
        }

        if (last_runtime == &runtime::current()) {
            if (update_required) update();
            return;
        }

        throw std::runtime_error(
            "attempt to use one runtime while registered to another"
        );
    }

    auto system_event::unlink() noexcept -> void {
        head->tail = tail;
        tail->head = head;

        head = this;
        tail = this;
    }

    auto system_event::update() -> void {
        runtime::current().update(*this);
    }

    auto system_event::wait(
        uint32_t events,
        void* state
    ) -> ext::task<detail::awaiter*> {
        set_events(events);

        TIMBER_TRACE("fd ({}) suspended", fd);

        try {
            co_return co_await detail::awaitable(awaiters, state);
            TIMBER_TRACE("fd ({}) resumed", fd);
        }
        catch (const task_canceled&) {
            TIMBER_DEBUG("fd ({}) canceled", fd);
            throw;
        }

        co_return nullptr;
    }
}
