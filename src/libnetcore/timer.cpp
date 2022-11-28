#include <netcore/timer.hpp>

#include <ext/except.h>
#include <timber/timber>

namespace netcore {
    auto timer::realtime() -> timer {
        return timer(CLOCK_REALTIME);
    }

    auto timer::monotonic() -> timer {
        return timer(CLOCK_MONOTONIC);
    }

    auto timer::boottime() -> timer {
        return timer(CLOCK_BOOTTIME);
    }

    auto timer::realtime_alarm() -> timer {
        return timer(CLOCK_REALTIME_ALARM);
    }

    auto timer::boottime_alarm() -> timer {
        return timer(CLOCK_BOOTTIME_ALARM);
    }

    timer::timer(int clockid) :
        descriptor(timerfd_create(clockid, TFD_NONBLOCK)),
        notification(descriptor, EPOLLIN)
    {
        if (!descriptor.valid()) {
            throw ext::system_error("failed to create timer");
        }

        TIMBER_TRACE("timer ({}) created", descriptor);
    }

    auto timer::disarm() -> void {
        set_time(0, 0);
        notification.cancel();

        TIMBER_TRACE("timer ({}) disarmed", descriptor);
    }

    auto timer::get_time() const -> itimerspec {
        auto spec = itimerspec();
        timerfd_gettime(descriptor, &spec);
        return spec;
    }

    auto timer::set(nanoseconds nsec, nanoseconds interval) -> void {
        TIMBER_TRACE(
            "timer ({}) set for {}ns ({}ns interval)",
            descriptor,
            nsec.count(),
            interval.count()
        );

        set_time(nsec.count(), interval.count());
    }

    auto timer::set_time(long value, long interval) const -> void {
        auto info = itimerspec {
            .it_interval = {
                .tv_sec = 0,
                .tv_nsec = interval
            },
            .it_value = {
                .tv_sec = 0,
                .tv_nsec = value
            }
        };

        timerfd_settime(descriptor, 0, &info, nullptr);
    }

    auto timer::wait() -> ext::task<std::uint64_t> {
        std::uint64_t expirations = 0;
        detail::awaiter* awaiters = nullptr;

        auto retval = -1;

        do {
            retval = read(descriptor, &expirations, sizeof(expirations));

            if (retval == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    awaiters = co_await notification.wait(0, &expirations);
                    if (expirations > 0) break;
                    continue;
                }

                throw ext::system_error("failed to read timer value");
            }

            notification.notify(expirations);
            if (awaiters) awaiters->assign(expirations);
        } while (retval != sizeof(expirations));

        TIMBER_TRACE("timer ({}) expirations: {}", descriptor, expirations);
        co_return expirations;
    }
}
