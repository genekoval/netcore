#pragma once

#include <netcore/fd.hpp>
#include <netcore/runtime.hpp>

#include <fmt/format.h>
#include <optional>
#include <sys/wait.h>

namespace netcore::proc {
    enum class code {
        none = 0,
        exited = CLD_EXITED,
        killed = CLD_KILLED,
        dumped = CLD_DUMPED,
        trapped = CLD_TRAPPED,
        stopped = CLD_STOPPED,
        continued = CLD_CONTINUED
    };

    auto to_string(code code) -> std::string_view;

    struct state {
        proc::code code;
        int status;
    };

    class process {
        friend struct fmt::formatter<process>;

        pid_t id = 0;
        fd descriptor;
        std::shared_ptr<runtime::event> event;
    public:
        process() = default;

        explicit process(pid_t pid);

        explicit operator bool() const noexcept;

        auto kill(int signal) const -> void;

        auto pid() const noexcept -> pid_t;

        auto wait(int states = WEXITED) -> ext::task<state>;
    };

    auto fork() -> process;
}

template <>
struct fmt::formatter<netcore::proc::state> : formatter<std::string_view> {
    template <typename FormatContext>
    auto format(netcore::proc::state state, FormatContext& ctx) {
        using netcore::proc::code;

        auto buffer = memory_buffer();
        auto it = std::back_inserter(buffer);

        switch (state.code) {
            case code::none: format_to(it, "{}", "<no return state>"); break;
            case code::exited:
                format_to(it, "exited with code {}", state.status);
                break;
            default:
                format_to(
                    it,
                    "{} by signal {}",
                    netcore::proc::to_string(state.code),
                    state.status
                );
                break;
        }

        return formatter<std::string_view>::format(
            {buffer.data(), buffer.size()},
            ctx
        );
    }
};

template <>
struct fmt::formatter<netcore::proc::process> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::proc::process& process, FormatContext& ctx) {
        return format_to(
            ctx.out() ,
            "process[{}] ({})",
            process.id,
            static_cast<int>(process.descriptor)
        );
    }
};
