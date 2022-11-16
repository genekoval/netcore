#include <netcore/detail/notifier.hpp>
#include <netcore/except.hpp>

#include <cassert>
#include <chrono>
#include <ext/except.h>
#include <timber/timber>

using namespace std::chrono_literals;

template <>
struct fmt::formatter<netcore::detail::notifier> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }

    auto format(const netcore::detail::notifier& notifier, auto& ctx) {
        return format_to(ctx.out(), "notifier ({})", notifier.descriptor);
    }
};

namespace {
    thread_local netcore::detail::notifier* instance_ptr = nullptr;
}

namespace netcore::detail {
    using clock = std::chrono::steady_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto notifier::instance() -> notifier& {
        assert(
            (instance_ptr != nullptr) &&
            "no active runtime in current thread"
        );

        return *instance_ptr;
    }

    notifier::notifier(const notifier_options& options) :
        events(std::make_unique<epoll_event[]>(options.max_events)),
        max_events(options.max_events),
        timeout(options.timeout),
        descriptor(epoll_create1(0))
    {
        if (!descriptor.valid()) {
            throw ext::system_error("epoll create failure");
        }

        if (instance_ptr) {
            throw std::runtime_error(
                "notifier cannot be created: already exists"
            );
        }

        instance_ptr = this;

        TIMBER_TRACE("{} created", *this);
    }

    notifier::~notifier() {
        instance_ptr = nullptr;
    }

    auto notifier::add(notification& notification) -> void {
        if (stat == notifier_status::force_shutdown) throw task_canceled();

        notifications.append(notification);

        auto event = epoll_event {
            .events = notification.events,
            .data = {
                .ptr = &notification
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_ADD,
            notification.fd,
            &event
        ) == -1) {
            TIMBER_DEBUG("{} failed to add entry ({})", *this, notification.fd);
            throw ext::system_error("failed to add entry to interest list");
        }

        TIMBER_TRACE("{} added entry ({})", *this, notification.fd);
    }

    auto notifier::empty() const noexcept -> bool {
        return notifications.empty() && pending.empty();
    }

    auto notifier::enqueue(awaiter& a) -> void {
        pending.enqueue(a);
    }

    auto notifier::enqueue(awaiter_queue& awaiters) -> void {
        pending.enqueue(awaiters);
    }

    auto notifier::one_or_empty() const noexcept -> bool {
        return notifications.one_or_empty();
    }

    auto notifier::remove(notification& notification) -> void {
        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_DEL,
            notification.fd,
            nullptr
        ) == -1) {
            TIMBER_DEBUG(
                "{} failed to remove entry ({})",
                *this,
                notification.fd
            );
            throw ext::system_error("failed to remove entry in interest list");
        }

        TIMBER_TRACE("{} removed entry ({})", *this, notification.fd);
    }

    auto notifier::resume_all() -> void {
        TIMBER_TRACE("{} resuming all", *this);

        auto* current = &notifications;

        do {
            auto* const next = current->head;
            current->resume();
            current = next;
        } while (current != &notifications);

        pending.resume();
    }

    auto notifier::run() -> void {
        if (stat != notifier_status::stopped) return;

        auto graceful_timeout = timeout;

        TIMBER_TRACE("{} starting up", *this);
        stat = notifier_status::running;

        while (!empty()) {
            auto timeout = static_cast<long>(-1);

            if (!pending.empty()) timeout = 0;
            else if (stat == notifier_status::graceful_shutdown) {
                timeout = graceful_timeout.count();
            }

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

            if (stat == notifier_status::graceful_shutdown) {
                graceful_timeout -= wait_time;

                if (graceful_timeout <= 0ms) {
                    TIMBER_DEBUG(
                        "{} graceful timeout reached: stopping",
                        *this
                    );

                    stop();
                }
            }

            if (ready == -1) {
                if (errno == EINTR) continue;
                TIMBER_DEBUG("{} wait failure", *this);
                throw ext::system_error("epoll wait failure");
            }

            for (auto i = 0; i < ready; ++i) {
                const auto& event = events[i];
                auto& notif = *static_cast<notification*>(event.data.ptr);

                notif.events = event.events;
                notif.resume();
            }

            pending.resume();
        }

        stat = notifier_status::stopped;
        TIMBER_TRACE("{} stopped", *this);
    }

    auto notifier::shutdown() noexcept -> void {
        if (stat == notifier_status::stopped) {
            TIMBER_DEBUG("{} shutdown request ignored: already stopped", *this);
            return;
        }

        if (stat == notifier_status::force_shutdown) {
            TIMBER_DEBUG(
                "{} shutdown request ignored: force shutdown in progress",
                *this
            );
            return;
        }

        TIMBER_TRACE("{} received shutdown request", *this);
        stat = notifier_status::graceful_shutdown;
        resume_all();
    }

    auto notifier::shutting_down() const noexcept -> bool {
        return
            stat == notifier_status::graceful_shutdown ||
            stat == notifier_status::force_shutdown;
    }

    auto notifier::status() const noexcept -> notifier_status {
        return stat;
    }

    auto notifier::stop() noexcept -> void {
        TIMBER_TRACE("{} received stop request", *this);
        stat = notifier_status::force_shutdown;
        resume_all();
    }

    auto notifier::update(notification& notification) -> void {
        auto event = epoll_event {
            .events = notification.events,
            .data = {
                .ptr = &notification
            }
        };

        if (epoll_ctl(
            descriptor,
            EPOLL_CTL_MOD,
            notification.fd,
            &event
        ) == -1) {
            TIMBER_DEBUG(
                "{} failed to modify entry ({})",
                *this,
                notification.fd
            );
            throw ext::system_error("failed to modify entry in interest list");
        }

        TIMBER_TRACE("{} updated entry ({})", *this, notification.fd);
    }
}
