#pragma once

#include "fd.h"
#include "system_event.hpp"

#include <fmt/core.h>
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

    struct state {
        code code;
        int status;
    };

    class process {
        friend struct fmt::formatter<process>;

        pid_t id = 0;
        fd descriptor;
        system_event event;
    public:
        process(pid_t pid);

        process() = default;

        explicit operator bool() const noexcept;

        auto kill(int signal) const -> void;

        auto pid() const noexcept -> pid_t;

        auto wait(int states = WEXITED) -> ext::task<state>;
    };

    auto fork() -> process;
}

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
