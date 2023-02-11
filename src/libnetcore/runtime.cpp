#include <netcore/async.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <cassert>
#include <chrono>
#include <ext/except.h>
#include <timber/timber>

using namespace std::chrono_literals;

template <>
struct fmt::formatter<netcore::runtime> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }

    auto format(const netcore::runtime& runtime, auto& ctx) {
        return format_to(ctx.out(), "runtime ({})", runtime.descriptor);
    }
};

namespace {
    thread_local netcore::runtime* current_ptr = nullptr;
}

namespace netcore {
    using clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto runtime::current() -> runtime& {
        assert(
            (current_ptr != nullptr) &&
            "no active runtime in current thread"
        );

        return *current_ptr;
    }

    runtime::runtime() : runtime(runtime_options{}) {}

    runtime::runtime(const runtime_options& options) :
        events(std::make_unique<epoll_event[]>(options.max_events)),
        max_events(options.max_events),
        timeout(options.timeout),
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
        run(true);
        current_ptr = nullptr;
    }

    auto runtime::add(system_event& event) -> void {
        system_events.append(event);

        auto ev = epoll_event {
            .events = event.events,
            .data = {
                .ptr = &event
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_ADD,
            event.fd,
            &ev
        ) == -1) {
            TIMBER_DEBUG("{} failed to add entry ({})", *this, event.fd);
            throw ext::system_error("failed to add entry to interest list");
        }

        TIMBER_TRACE("{} added entry ({})", *this, event.fd);
    }

    auto runtime::cancel() -> void {
        TIMBER_TRACE("{} canceling tasks", *this);

        pending.cancel();

        auto* current = &system_events;

        do {
            current->cancel();
            current = current->head;
        } while (current != &system_events);
    }

    auto runtime::enqueue(detail::awaiter& a) -> void {
        pending.enqueue(a);
    }

    auto runtime::enqueue(detail::awaiter_queue& awaiters) -> void {
        pending.enqueue(awaiters);
    }

    auto runtime::notify() -> void {
        auto* current = &system_events;

        do {
            current->notify();
            current = current->head;
        } while (current != &system_events);
    }

    auto runtime::one_or_empty() const noexcept -> bool {
        return system_events.one_or_empty();
    }

    auto runtime::remove(system_event& event) -> void {
        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_DEL,
            event.fd,
            nullptr
        ) == -1) {
            TIMBER_DEBUG(
                "{} failed to remove entry ({})",
                *this,
                event.fd
            );
            throw ext::system_error("failed to remove entry in interest list");
        }

        TIMBER_TRACE("{} removed entry ({})", *this, event.fd);
    }

    auto runtime::resume_all() -> void {
        TIMBER_TRACE("{} resuming all", *this);

        auto* current = &system_events;

        do {
            auto* const next = current->head;
            current->resume();
            current = next;
        } while (current != &system_events);

        pending.resume();
    }

    auto runtime::run() -> void {
        run(false);
    }

    auto runtime::run(bool eol) -> void {
        if (eol) {
            TIMBER_TRACE("{} performing final end-of-life run", *this);
            stat = runtime_status::force_shutdown;
        }
        else if (stat == runtime_status::stopped) {
            TIMBER_TRACE("{} starting up", *this);
            stat = runtime_status::running;
        }

        // Make a local, mutable copy for this run.
        auto graceful_timeout = timeout;

        // Ensure the runtime goes through at least one loop after graceful
        // shutdown begins even if the timeout is zero.
        auto shutdown_ran = false;

        // When the status has been set to 'force shutdown', all remaining
        // tasks will be in the 'pending' queue.
        while (!(stat == runtime_status::force_shutdown && pending.empty())) {
            long timeout = -1;

            if (!pending.empty()) timeout = 0;
            else if (stat == runtime_status::graceful_shutdown) {
                timeout = graceful_timeout.count();
            }

            TIMBER_TRACE("{} waiting for events", *this);

            const auto wait_started = clock::now();

            const auto ready = epoll_wait(
                descriptor,
                events.get(),
                max_events,
                timeout
            );

            const auto wait_time = duration_cast<milliseconds>(
                clock::now() - wait_started
            );

            TIMBER_TRACE(
                "{} waited for {:L}ms ({:L} ready)",
                *this,
                wait_time.count(),
                ready
            );

            if (stat == runtime_status::graceful_shutdown) {
                graceful_timeout -= wait_time;

                if (shutdown_ran && graceful_timeout <= 0ms) {
                    TIMBER_DEBUG(
                        "{} graceful timeout reached: stopping",
                        *this
                    );

                    stop();
                }

                shutdown_ran = true;
            }

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

        stat = runtime_status::stopped;
        TIMBER_TRACE("{} stopped", *this);
    }

    auto runtime::run(ext::task<>&& task) -> void {
        auto exception = std::exception_ptr();

        run_main_task(std::forward<ext::task<>>(task), exception);
        run();

        if (exception) std::rethrow_exception(exception);
    }

    auto runtime::run_main_task(
        ext::task<>&& task,
        std::exception_ptr& exception
    ) -> ext::detached_task {
        const auto t = std::forward<ext::task<>>(task);

        // Wait for the runtime to start.
        co_await yield();

        TIMBER_DEBUG("{} main task starting", *this);

        try {
            co_await t;
        }
        catch (...) {
            exception = std::current_exception();
        }

        TIMBER_DEBUG("{} main task complete", *this);

        stop();
    }

    auto runtime::shutdown() noexcept -> void {
        if (stat == runtime_status::stopped) {
            TIMBER_DEBUG("{} shutdown request ignored: already stopped", *this);
            return;
        }

        if (stat == runtime_status::force_shutdown) {
            TIMBER_DEBUG(
                "{} shutdown request ignored: force shutdown in progress",
                *this
            );
            return;
        }

        TIMBER_TRACE("{} received shutdown request", *this);
        stat = runtime_status::graceful_shutdown;
        notify();
    }

    auto runtime::shutting_down() const noexcept -> bool {
        return
            stat == runtime_status::graceful_shutdown ||
            stat == runtime_status::force_shutdown;
    }

    auto runtime::status() const noexcept -> runtime_status {
        return stat;
    }

    auto runtime::stop() noexcept -> void {
        TIMBER_TRACE("{} received stop request", *this);
        stat = runtime_status::force_shutdown;
        cancel();
    }

    auto runtime::update(system_event& event) -> void {
        auto ev = epoll_event {
            .events = event.events,
            .data = {
                .ptr = &event
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_MOD,
            event.fd,
            &ev
        ) == -1) {
            TIMBER_DEBUG(
                "{} failed to modify entry ({})",
                *this,
                event.fd
            );
            throw ext::system_error("failed to modify entry in interest list");
        }

        TIMBER_TRACE("{} updated entry ({})", *this, event.fd);
    }
}
