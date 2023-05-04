#pragma once

namespace netcore {
    auto flags(int fd) -> int;

    auto flags(int fd, int flags) -> void;

    auto make_nonblocking(int fd) -> void;
}
