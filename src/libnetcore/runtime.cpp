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

    auto runtime::add(awaiter* a) -> void {
        auto ev = epoll_event {
            .events = a->events() | permanent_events,
            .data = {
                .ptr = a
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_ADD,
            a->fd(),
            &ev
        ) == -1) {
            TIMBER_DEBUG("{} failed to add entry ({})", *this, a->fd());
            throw ext::system_error("Failed to add entry to interest list");
        }

        ++awaiters;

        TIMBER_TRACE("{} added entry ({})", *this, a->fd());
    }

    auto runtime::modify(awaiter* a) -> void {
        auto ev = epoll_event {
            .events = a->events() | permanent_events,
            .data = {
                .ptr = a
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_MOD,
            a->fd(),
            &ev
        ) == -1) {
            TIMBER_DEBUG("{} failed to modify entry ({})", *this, a->fd());
            throw ext::system_error("Failed to modify runtime entry");
        }

        TIMBER_TRACE("{} modified entry ({})", *this, a->fd());
    }

    auto runtime::enqueue(detail::awaiter& a) -> void {
        pending.enqueue(a);
    }

    auto runtime::enqueue(detail::awaiter_queue& awaiters) -> void {
        pending.enqueue(awaiters);
    }

    auto runtime::remove(int fd) -> void {
        --awaiters;

        if (epoll_ctl(descriptor, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            TIMBER_DEBUG("{} failed to remove entry ({})", *this, fd);
            throw ext::system_error("Failed to remove entry in interest list");
        }

        TIMBER_TRACE("{} removed entry ({})", *this, fd);
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
                const auto& event = events[i];
                auto* a = static_cast<awaiter*>(event.data.ptr);
                a->resume(event.events);
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

    runtime::awaiter::awaiter(int fd) noexcept : descriptor(fd) {}

    runtime::awaiter::awaiter(awaiter&& other) :
        descriptor(std::exchange(other.descriptor, -1)),
        submission(std::exchange(other.submission, 0)),
        completion(std::exchange(other.completion, 0)),
        coroutine(std::exchange(other.coroutine, nullptr))
    {
        if (coroutine) runtime::current().modify(this);
    }

    runtime::awaiter::~awaiter() {
        if (coroutine) {
            try {
                runtime::current().remove(descriptor);
            }
            catch (const std::exception& ex) {
                TIMBER_ERROR(ex.what());
            }
        }
    }

    auto runtime::awaiter::operator=(awaiter&& other) -> awaiter& {
        if (std::addressof(other) != this) {
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
        }

        return *this;
    }

    auto runtime::awaiter::add(std::uint32_t events) -> void {
        set(submission | events);
    }

    auto runtime::awaiter::awaiting() const noexcept -> bool {
        return coroutine != nullptr;
    }

    auto runtime::awaiter::await_ready() const noexcept -> bool {
        return canceled;
    }

    auto runtime::awaiter::await_suspend(
        std::coroutine_handle<> coroutine
    ) -> void {
        TIMBER_TRACE("fd ({}) suspended", descriptor);

        runtime::current().add(this);
        this->coroutine = coroutine;
    }

    auto runtime::awaiter::await_resume() noexcept -> std::uint32_t {
        TIMBER_TRACE(
            "fd ({}) {}",
            descriptor,
            canceled ? "canceled" : "resumed"
        );

        if (coroutine) runtime::current().remove(descriptor);

        coroutine = nullptr;
        canceled = false;

        return std::exchange(completion, 0);
    }

    auto runtime::awaiter::cancel() -> void {
        canceled = true;
        if (coroutine) coroutine.resume();
    }

    auto runtime::awaiter::events() const noexcept -> std::uint32_t {
        return submission;
    }

    auto runtime::awaiter::fd() const noexcept -> int {
        return descriptor;
    }

    auto runtime::awaiter::resume(std::uint32_t events) -> void {
        completion = events;
        if (coroutine) coroutine.resume();
    }

    auto runtime::awaiter::set(std::uint32_t events) -> void {
        const auto submission = this->submission;
        this->submission = events;

        if (coroutine && this->submission != submission) {
            runtime::current().modify(this);
        }
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
