#include <netcore/signalfd.h>

#include <ext/except.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <timber/timber>
#include <unistd.h>

namespace netcore {
    signalfd::signalfd(int descriptor) : descriptor(descriptor) {
        TIMBER_DEBUG("signalfd ({}) created", descriptor);
    }

    signalfd::operator int() const { return descriptor; }

    auto signalfd::create(std::span<const int> signals) -> signalfd {
        auto mask = sigset_t();
        sigemptyset(&mask);

        for (const auto& sig: signals) sigaddset(&mask, sig);

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
            throw ext::system_error("sigprocmask failure");
        }

        auto descriptor = ::signalfd(-1, &mask, 0);
        if (descriptor == -1) {
            throw ext::system_error("signalfd create failure");
        }

        return descriptor;
    }

    auto signalfd::signal() const -> std::uint32_t {
        constexpr auto infolen = sizeof(signalfd_siginfo);

        auto info = signalfd_siginfo();

        const auto bytes = read(descriptor, &info, infolen);
        if (bytes != infolen) {
            throw ext::system_error("Failure to read signalfd signal");
        }

        return info.ssi_signo;
    }
}
