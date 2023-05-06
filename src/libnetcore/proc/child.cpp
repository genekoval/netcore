#include <netcore/proc/child.hpp>

#include <ext/string.h>

namespace netcore::proc {
    child::child(process&& process, stdio_streams&& stdio) :
        proc::process(std::forward<proc::process>(process)),
        stdio(std::forward<stdio_streams>(stdio))
    {}

    auto child::check_return_code() -> ext::task<> {
        const auto state = co_await wait();
        if (state.code == code::exited && state.status == 0) co_return;

        auto& stream = err();

        if (stream.is<piped>()) {
            const auto message = co_await stream.read<std::string>();

            throw subprocess_error(
                pid(),
                state,
                std::string(ext::trim(message))
            );
        }

        throw subprocess_error(pid(), state);
    }

    auto child::err() -> stdio_stream& {
        return stdio[2];
    }

    auto child::in() -> stdio_stream& {
        return stdio[0];
    }

    auto child::out() -> stdio_stream& {
        return stdio[1];
    }

    auto child::read() -> ext::task<std::string> {
        co_await check_return_code();
        co_return co_await out().read<std::string>();
    }

    subprocess_error::subprocess_error(pid_t pid, proc::state state) :
        runtime_error(fmt::format("Process with PID {} {}", pid, state)),
        process_state(state)
    {}

    subprocess_error::subprocess_error(
        pid_t pid,
        proc::state state,
        std::string&& err
    ) :
        runtime_error(fmt::format(
            "Process with PID {} {}: {}", pid, state, err
        )),
        process_state(state),
        err_output(std::forward<std::string>(err))

    {}

    auto subprocess_error::err() const noexcept -> std::string_view {
        return err_output;
    }

    auto subprocess_error::state() const noexcept -> proc::state {
        return process_state;
    }
}
