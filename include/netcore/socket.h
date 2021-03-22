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

        auto read(void* buffer, std::size_t len) const -> std::size_t;

        auto sendfile(const fd& descriptor, std::size_t count) const -> void;

        auto valid() const -> bool;

        auto write(const void* data, std::size_t len) const -> std::size_t;
    };

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream&;
}
