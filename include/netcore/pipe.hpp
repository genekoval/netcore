#pragma once

#include "fd.hpp"

namespace netcore {
    class pipe {
        fd read_end;
        fd write_end;
    public:
        pipe();

        pipe(const pipe&) = delete;

        pipe(pipe&&) = default;

        auto operator=(const pipe&) -> pipe& = delete;

        auto operator=(pipe&&) -> pipe& = default;

        auto read() -> fd;

        auto write() -> fd;
    };
}
