#include <netcore/except.hpp>
#include <netcore/proc/process.hpp>

#include <ext/except.h>
#include <linux/pidfd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <timber/timber>
#include <unistd.h>

namespace {
    auto to_code(int n) -> netcore::proc::code {
        using enum netcore::proc::code;

        switch (n) {
            case CLD_EXITED: return exited;
            case CLD_KILLED: return killed;
            case CLD_DUMPED: return dumped;
            case CLD_TRAPPED: return trapped;
            case CLD_STOPPED: return stopped;
            case CLD_CONTINUED: return continued;
            default:
                TIMBER_WARNING("Unknown process state code: {}", n);
                return none;
        }
    }
}

namespace netcore::proc {
    auto to_string(code code) -> std::string_view {
        switch (code) {
            case code::none: return "<none>";
            case code::exited: return "exited";
            case code::killed: return "killed";
            case code::dumped: return "dumped";
            case code::trapped: return "trapped";
            case code::stopped: return "stopped";
            case code::continued: return "continued";
            default: return "<unknown>";
        }
    }

    process::process(pid_t pid) :
        id(pid),
        descriptor(syscall(SYS_pidfd_open, id, PIDFD_NONBLOCK)),
        event(runtime::event::create(descriptor, EPOLLIN))
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
        str = str ? str : "invalid signal";

        TIMBER_DEBUG(
            "Sending signal ({}: {}) to process with PID {}",
            signal,
            str,
            id
        );

        if (::kill(id, signal) == -1) {
            throw ext::system_error(fmt::format(
                "failed to send signal ({}: {}) to process with PID {}",
                signal,
                str,
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
                    if (!co_await event->in()) throw task_canceled();
                }
                else throw ext::system_error(fmt::format(
                    "failed to wait for process with PID {}",
                    id
                ));
            }
        } while (siginfo.si_pid == 0);

        auto result = state {
            .code = to_code(siginfo.si_code),
            .status = siginfo.si_status
        };

        TIMBER_DEBUG("{} {}", *this, result);

        co_return result;
    }

    auto fork() -> process {
        auto pid = ::fork();

        if (pid < 0) throw ext::system_error("failed to create child process");
        if (pid > 0) return process(pid);

        return process();
    }
}
