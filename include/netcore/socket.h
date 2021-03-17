#pragma once

#include <netcore/fd.h>

#include <sstream>
#include <string>

namespace netcore {
    class socket {
        fd sockfd;
    public:
        socket() = default;
        socket(int fd);
        socket(int domain, int type);

        operator int() const;

        auto end() const -> void;
        auto recv(void* buffer, std::size_t len) const -> std::size_t;
        auto send(const void* data, std::size_t len) const -> void;
        auto valid() const -> bool;
    };

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream&;
}
