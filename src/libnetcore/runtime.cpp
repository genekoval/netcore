#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <cassert>
#include <chrono>
#include <ext/except.h>

namespace {
    constexpr auto permanent_events = EPOLLET;

    thread_local netcore::runtime* current_runtime = nullptr;
}

namespace netcore {
    auto runtime::active() -> bool {
        return current_runtime != nullptr;
    }

    auto runtime::current() -> runtime& {
        assert(
            (current_runtime != nullptr) &&
            "no active runtime in current thread"
        );

        return *current_runtime;
    }

    runtime::runtime(int max_events) :
        events(std::make_unique<epoll_event[]>(max_events)),
        max_events(max_events),
        descriptor(epoll_create1(EPOLL_CLOEXEC))
    {
        if (!descriptor.valid()) {
            throw ext::system_error("epoll create failure");
        }

        if (current_runtime) {
            throw std::runtime_error(
                "runtime cannot be created: already exists"
            );
        }

        current_runtime = this;

        TIMBER_TRACE("{} created", *this);
    }

    runtime::~runtime() {
        current_runtime = nullptr;
        TIMBER_TRACE("{} destroyed", *this);
    }

    auto runtime::add(runtime::event* event) -> void {
        auto ev = epoll_event {
            .events = event->events | permanent_events,
            .data = {
                .ptr = event
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_ADD,
            event->fd(),
            &ev
        ) == -1) {
            TIMBER_DEBUG("{} failed to add entry ({})", *this, event->fd());
            throw ext::system_error("Failed to add entry to interest list");
        }

        TIMBER_TRACE("{} added entry ({})", *this, event->fd());
    }

    auto runtime::modify(runtime::event* event) -> void {
        auto ev = epoll_event {
            .events = event->events | permanent_events,
            .data = {
                .ptr = event
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_MOD,
            event->fd(),
            &ev
        ) == -1) {
            TIMBER_DEBUG("{} failed to modify entry ({})", *this, event->fd());
            throw ext::system_error("Failed to modify runtime entry");
        }

        TIMBER_TRACE("{} modified entry ({})", *this, event->fd());
    }

    auto runtime::enqueue(detail::awaiter& a) -> void {
        pending.enqueue(a);
    }

    auto runtime::enqueue(detail::awaiter_queue& awaiters) -> void {
        pending.enqueue(awaiters);
    }

    auto runtime::remove(int fd) const noexcept -> std::error_code {
        if (epoll_ctl(descriptor, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            auto error = std::error_code(errno, std::generic_category());

            TIMBER_DEBUG(
                "{} failed to remove entry ({}): {}",
                *this,
                fd,
                error.message()
            );

            return error;
        }

        TIMBER_TRACE("{} removed entry ({})", *this, fd);

        return {};
    }

    auto runtime::run() -> void {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;

        using clock = std::chrono::steady_clock;

        TIMBER_TRACE("{} starting up", *this);

        while (awaiters > 0 || !pending.empty()) {
            TIMBER_TRACE(
                "{} {:L} task{} waiting for events",
                *this,
                awaiters,
                awaiters == 1 ? "" : "s"
            );

            const auto wait_started = clock::now();

            const auto ready = epoll_wait(
                descriptor,
                events.get(),
                max_events,
                pending.empty() ? -1 : 0
            );

            const auto wait_time = duration_cast<milliseconds>(
                clock::now() - wait_started
            );

            TIMBER_TRACE(
                "{} waited for {:L}ms ({:L} ready / {:L} total)",
                *this,
                wait_time.count(),
                ready,
                awaiters
            );

            if (ready == -1) {
                if (errno == EINTR) continue;
                TIMBER_DEBUG("{} wait failure", *this);
                throw ext::system_error("epoll wait failure");
            }

            for (auto i = 0; i < ready; ++i) {
                const auto& current = events[i];
                auto& event = *static_cast<runtime::event*>(current.data.ptr);
                event.resume(current.events);
            }

            TIMBER_DEBUG(
                "{} pending tasks to resume: {:L}",
                *this,
                pending.size()
            );

            pending.resume();
        }

        TIMBER_TRACE("{} stopped", *this);
    }

    runtime::event::event(int fd, std::uint32_t events) noexcept :
        descriptor(fd),
        events(events)
    {
        runtime::current().add(this);
        this->events = 0;
    }

    auto runtime::event::cancel() -> void {
        const auto handle = shared_from_this();
        canceled = true;

        if (awaiting_in) awaiting_in.resume();
        if (awaiting_out) awaiting_out.resume();
    }

    auto runtime::event::create(
        int fd,
        std::uint32_t events
    ) noexcept -> std::shared_ptr<event> {
        return std::shared_ptr<event>(new event(fd, events));
    }

    auto runtime::event::fd() const noexcept -> int {
        return descriptor;
    }

    auto runtime::event::in() noexcept -> awaitable {
        return awaitable(*this, awaiting_in);
    }

    auto runtime::event::out() noexcept -> awaitable {
        return awaitable(*this, awaiting_out);
    }

    auto runtime::event::remove() const noexcept -> std::error_code {
        return runtime::current().remove(descriptor);
    }

    auto runtime::event::resume(std::uint32_t events) -> void {
        const auto handle = shared_from_this();
        received = events;

        if (this->events) {
            if (awaiting_out && (
                ((this->events & EPOLLIN) && (events & EPOLLIN)) ||
                ((this->events & EPOLLOUT) && (events & EPOLLOUT))
            )) awaiting_out.resume();

            return;
        }

        if ((events & EPOLLIN) && awaiting_in) awaiting_in.resume();
        if ((events & EPOLLOUT) && awaiting_out) awaiting_out.resume();
    }

    runtime::event::awaitable::awaitable(
        runtime::event& event,
        std::coroutine_handle<>& coroutine
    ) noexcept :
        event(event),
        coroutine(coroutine)
    {}

    runtime::event::awaitable::~awaitable() {
        if (!coroutine) return;

        coroutine = nullptr;

        if (!(event.awaiting_in || event.awaiting_out)) {
            --runtime::current().awaiters;
        }
    }

    auto runtime::event::awaitable::await_ready() const noexcept -> bool {
        return event.canceled;
    }

    auto runtime::event::awaitable::await_suspend(
        std::coroutine_handle<> coroutine
    ) -> void {
        TIMBER_TRACE("fd ({}) suspended", event.descriptor);

        if (!(event.awaiting_in || event.awaiting_out)) {
            ++runtime::current().awaiters;
        }

        this->coroutine = coroutine;
    }

    auto runtime::event::awaitable::await_resume() noexcept -> std::uint32_t {
        if (coroutine) {
            coroutine = nullptr;

            if (!(event.awaiting_in || event.awaiting_out)) {
                --runtime::current().awaiters;
            }
        }


        const auto canceled = event.canceled;
        if (!(event.awaiting_in || event.awaiting_out)) event.canceled = false;

        TIMBER_TRACE(
            "fd ({}) {}",
            event.descriptor,
            canceled ? "canceled" : "resumed"
        );

        return canceled ? 0 : event.received;
    }

    auto run() -> void {
        if (current_runtime) current_runtime->run();
        else runtime().run();
    }

    auto yield() -> ext::task<> {
        class awaitable {
            detail::awaiter awaiter;
        public:
            auto await_ready() const noexcept -> bool { return false; }

            auto await_suspend(std::coroutine_handle<> coroutine) -> void {
                awaiter.coroutine = coroutine;
                runtime::current().enqueue(awaiter);
            }

            auto await_resume() -> void {}
        };

        co_await awaitable();
    }
}
