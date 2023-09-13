#include <netcore/except.hpp>
#include <netcore/signalfd.h>

#include <ext/except.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <timber/timber>
#include <unistd.h>

namespace netcore {
    signalfd::signalfd(int descriptor) :
        descriptor(descriptor),
        event(runtime::event::create(this->descriptor, EPOLLIN))
    {
        TIMBER_TRACE("signalfd ({}) created", descriptor);
    }

    signalfd::operator int() const { return descriptor; }

    auto signalfd::create(std::span<const int> signals) -> signalfd {
        auto mask = sigset_t();
        sigemptyset(&mask);

        for (const auto& sig: signals) sigaddset(&mask, sig);

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
            throw ext::system_error("sigprocmask failure");
        }

        auto fd = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (fd == -1) {
            throw ext::system_error("Failed to create signalfd");
        }

        return signalfd(fd);
    }

    auto signalfd::wait_for_signal() -> ext::task<int> {
        while (true) {
            auto info = signalfd_siginfo();

            const auto bytes = ::read(descriptor, &info, sizeof(info));

            if (bytes == sizeof(info)) co_return info.ssi_signo;

            if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (!co_await event->in()) co_return 0;
            }
            else throw ext::system_error("Failed to read signal info");
        }
    }
}
