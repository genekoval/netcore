#include <netcore/process.hpp>

#include <ext/except.h>
#include <linux/pidfd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <timber/timber>
#include <unistd.h>

namespace {
    auto get_state(int state) -> netcore::proc::code {
        using enum netcore::proc::code;

        switch (state) {
            case CLD_EXITED: return exited;
            case CLD_KILLED: return killed;
            case CLD_DUMPED: return dumped;
            case CLD_TRAPPED: return trapped;
            case CLD_STOPPED: return stopped;
            case CLD_CONTINUED: return continued;
            default:
                TIMBER_WARNING("Unknown process state code: {}", state);
                return none;
        }
    }
}

namespace netcore::proc {
    process::process(pid_t pid) :
        id(pid),
        descriptor(syscall(SYS_pidfd_open, id, PIDFD_NONBLOCK)),
        event(descriptor, EPOLLIN)
    {
        if (!descriptor.valid()) {
            throw ext::system_error("failed to open pid fd");
        }

        TIMBER_TRACE("{} created", *this);
    }

    process::operator bool() const noexcept {
        return id != 0;
    }

    auto process::kill(int signal) const -> void {
        const auto* str = strsignal(signal);

        TIMBER_DEBUG(
            "sending signal ({}: {}) to process with PID {}",
            signal,
            str ? str : "invalid signal",
            id
        );

        if (::kill(id, signal) == -1) {
            throw ext::system_error(fmt::format(
                "failed to send signal ({}: {}) to process with PID {}",
                signal,
                str ? str : "invalid signal",
                id
            ));
        }
    }

    auto process::pid() const noexcept -> pid_t {
        return id;
    }

    auto process::wait(int states) -> ext::task<state> {
        auto siginfo = siginfo_t();

        do {
            if (waitid(P_PIDFD, descriptor, &siginfo, states) == -1) {
                if (errno == EAGAIN) {
                    co_await event.wait(0, nullptr);
                }
                else {
                    throw ext::system_error(fmt::format(
                        "failed to wait for process with pid {}",
                        id
                    ));
                }
            }
        } while (siginfo.si_pid == 0);

        co_return state {
            .code = get_state(siginfo.si_code),
            .status = siginfo.si_status
        };
    }

    auto fork() -> process {
        auto pid = ::fork();

        if (pid < 0) throw ext::system_error("failed to create child process");
        if (pid > 0) return process(pid);

        return process();
    }
}
