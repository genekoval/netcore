#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <cassert>
#include <chrono>
#include <ext/except.h>

namespace {
    thread_local netcore::runtime* current_ptr = nullptr;
}

namespace netcore {
    auto runtime::active() -> bool {
        return current_ptr != nullptr;
    }

    auto runtime::current() -> runtime& {
        assert(
            (current_ptr != nullptr) &&
            "no active runtime in current thread"
        );

        return *current_ptr;
    }

    runtime::runtime(int max_events) :
        events(std::make_unique<epoll_event[]>(max_events)),
        max_events(max_events),
        descriptor(epoll_create1(EPOLL_CLOEXEC))
    {
        if (!descriptor.valid()) {
            throw ext::system_error("epoll create failure");
        }

        if (current_ptr) {
            throw std::runtime_error(
                "runtime cannot be created: already exists"
            );
        }

        current_ptr = this;

        TIMBER_TRACE("{} created", *this);
    }

    runtime::~runtime() {
        current_ptr = nullptr;
        TIMBER_TRACE("{} destroyed", *this);
    }

    auto runtime::add(system_event* event) -> void {
        auto ev = epoll_event {
            .events = event->events,
            .data = {
                .ptr = event
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_ADD,
            event->fd,
            &ev
        ) == -1) {
            TIMBER_DEBUG("{} failed to add entry ({})", *this, event->fd);
            throw ext::system_error("failed to add entry to interest list");
        }

        TIMBER_TRACE("{} added entry ({})", *this, event->fd);

        event->registered = true;
        ++system_events;
    }

    auto runtime::enqueue(detail::awaiter& a) -> void {
        pending.enqueue(a);
    }

    auto runtime::enqueue(detail::awaiter_queue& awaiters) -> void {
        pending.enqueue(awaiters);
    }

    auto runtime::remove(system_event* event) -> void {
        --system_events;

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_DEL,
            event->fd,
            nullptr
        ) == -1) {
            TIMBER_DEBUG(
                "{} failed to remove entry ({})",
                *this,
                event->fd
            );
            throw ext::system_error("failed to remove entry in interest list");
        }

        TIMBER_TRACE("{} removed entry ({})", *this, event->fd);

        event->registered = false;
    }

    auto runtime::run() -> void {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;

        using clock = std::chrono::steady_clock;

        TIMBER_TRACE("{} starting up", *this);

        while (!pending.empty() || system_events > 0) {
            TIMBER_TRACE("{} waiting for events", *this);

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
                system_events
            );

            if (ready == -1) {
                if (errno == EINTR) continue;
                TIMBER_DEBUG("{} wait failure", *this);
                throw ext::system_error("epoll wait failure");
            }

            for (auto i = 0; i < ready; ++i) {
                const auto& ev = events[i];
                auto& event = *static_cast<system_event*>(ev.data.ptr);

                event.events = ev.events;
                event.resume();
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

    auto run() -> void {
        if (current_ptr) current_ptr->run();
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
