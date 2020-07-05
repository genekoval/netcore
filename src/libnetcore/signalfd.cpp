#include <netcore/signalfd.h>

#include <ext/except.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <timber/timber>
#include <unistd.h>

namespace netcore {
    signalfd::signalfd(int fd) : sock(fd) {}

    auto signalfd::create(const std::vector<int>& signals) -> signalfd {
        auto mask = sigset_t();
        sigemptyset(&mask);

        for (const auto& sig: signals) sigaddset(&mask, sig);

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
            throw ext::system_error("sigprocmask failure");
        }

        auto sig = signalfd(::signalfd(-1, &mask, 0));
        if (!sig.sock.valid()) {
            throw ext::system_error("signalfd create failure");
        }

        TRACE() << sig.sock << " signalfd created";

        return sig;
    }

    auto signalfd::fd() const -> int { return sock.fd(); }

    auto signalfd::signal() const -> uint32_t {
        auto fdsi = signalfd_siginfo();
        const auto infolen = sizeof(signalfd_siginfo);

        const auto bytes_read = read(fd(), &fdsi, infolen);
        if (bytes_read != infolen) {
            throw ext::system_error("Failure to read signalfd signal");
        }

        return fdsi.ssi_signo;
    }
}
