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
        event(this->descriptor, EPOLLIN)
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
            throw ext::system_error("signalfd create failure");
        }

        return signalfd(fd);
    }

    auto signalfd::read(
        void* buffer,
        std::size_t len
    ) -> ext::task<void> {
        std::size_t total = 0;

        do {
            const auto bytes = ::read(descriptor, buffer, len);

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await event.wait(0, nullptr);
                    continue;
                }

                throw ext::system_error("failed to read signalfd data");
            }

            total += bytes;
        }
        while (total < len);
    }

    auto signalfd::wait_for_signal() -> ext::task<int> {
        constexpr auto infolen = sizeof(signalfd_siginfo);

        auto info = signalfd_siginfo();
        co_await read(&info, infolen);

        co_return info.ssi_signo;
    }
}
