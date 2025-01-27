#include <netcore/timer.hpp>

#include <ext/except.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <timber/timber>

using namespace std::chrono_literals;

using std::chrono::floor;
using std::chrono::nanoseconds;
using std::chrono::seconds;

namespace {
    auto from_nsec(nanoseconds ns) -> timespec {
        const auto s = floor<seconds>(ns);
        const auto remainder = ns - s;

        return {.tv_sec = s.count(), .tv_nsec = remainder.count()};
    }

    auto to_nsec(const timespec& spec) -> nanoseconds {
        return seconds(spec.tv_sec) + nanoseconds(spec.tv_nsec);
    }
}

namespace netcore {
    auto timer::realtime() -> timer { return timer(CLOCK_REALTIME); }

    auto timer::monotonic() -> timer { return timer(CLOCK_MONOTONIC); }

    auto timer::boottime() -> timer { return timer(CLOCK_BOOTTIME); }

    auto timer::realtime_alarm() -> timer {
        return timer(CLOCK_REALTIME_ALARM);
    }

    auto timer::boottime_alarm() -> timer {
        return timer(CLOCK_BOOTTIME_ALARM);
    }

    timer::timer(int clockid) :
        descriptor(timerfd_create(clockid, TFD_NONBLOCK | TFD_CLOEXEC)),
        event(runtime::event::create(descriptor, EPOLLIN)) {
        if (!descriptor.valid()) {
            throw ext::system_error("failed to create timer");
        }

        TIMBER_TRACE("{} created", *this);
    }

    auto timer::disarm() -> void {
        set(time {.value = nanoseconds::zero(), .interval = nanoseconds::zero()}
        );

        event->cancel();
    }

    auto timer::get() const -> time {
        auto spec = itimerspec();

        if (timerfd_gettime(descriptor, &spec) == -1) {
            throw ext::system_error("failed to read time");
        }

        return {
            .value = to_nsec(spec.it_value),
            .interval = to_nsec(spec.it_interval)};
    }

    auto timer::set(nanoseconds value) const -> void {
        set(time {.value = value});
    }

    auto timer::set(const time& t) const -> void {
        const auto spec = itimerspec {
            .it_interval = from_nsec(t.interval),
            .it_value = from_nsec(t.value)};

        if (timerfd_settime(descriptor, 0, &spec, nullptr) == -1) {
            throw ext::system_error("failed to set timer");
        }

        TIMBER_DEBUG(
            "{} set for {:L}ns{}",
            *this,
            t.value.count(),
            t.interval == nanoseconds::zero()
                ? ""
                : fmt::format("; {:L}ns interval", t.interval.count())
        );
    }

    auto timer::wait() -> ext::task<std::uint64_t> {
        std::uint64_t expirations = 0;
        auto retval = -1;

        do {
            retval = read(descriptor, &expirations, sizeof(expirations));

            if (retval == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (co_await event->in()) continue;
                    else {
                        TIMBER_DEBUG("{} disarmed", *this);
                        co_return 0;
                    }
                }

                throw ext::system_error("Failed to read timer value");
            }
        } while (retval != sizeof(expirations));

        TIMBER_DEBUG("{} expirations: {:L}", *this, expirations);
        co_return expirations;
    }

    auto timer::waiting() const noexcept -> bool {
        return event->awaiting_in != nullptr;
    }
}
