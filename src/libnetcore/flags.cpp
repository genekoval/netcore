#include <netcore/flags.hpp>

#include <ext/except.h>
#include <fcntl.h>
#include <fmt/core.h>

namespace netcore {
    auto flags(int fd) -> int {
        auto flags = fcntl(fd, F_GETFL, 0);

        if (flags == -1) {
            throw ext::system_error(
                fmt::format("Failed to get flags for file descriptor ({})", fd)
            );
        }

        return flags;
    }

    auto flags(int fd, int flags) -> void {
        const auto new_flags = netcore::flags(fd) | flags;

        if (fcntl(fd, F_SETFL, new_flags) == -1) {
            throw ext::system_error(
                fmt::format("Failed to set flags for file descriptor ({})", fd)
            );
        }
    }

    auto make_nonblocking(int fd) -> void { flags(fd, O_NONBLOCK); }
}
