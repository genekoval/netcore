#pragma once

#include "process.hpp"
#include "stdio.hpp"

namespace netcore::proc {
    class child : public process {
        stdio_streams stdio;
    public:
        child(process&& process, stdio_streams&& stdio);

        auto check_return_code() -> ext::task<>;

        auto err() -> stdio_stream&;

        auto in() -> stdio_stream&;

        auto out() -> stdio_stream&;

        auto read() -> ext::task<std::string>;
    };

    class subprocess_error : public std::runtime_error {
        proc::state process_state;
        std::string err_output;
    public:
        subprocess_error(pid_t pid, proc::state state);

        subprocess_error(pid_t pid, proc::state state, std::string&& err);

        auto err() const noexcept -> std::string_view;

        auto state() const noexcept -> proc::state;
    };
}
