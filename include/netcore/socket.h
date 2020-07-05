#pragma once

#include <string>
#include <sstream>

namespace netcore {
    class socket {
        int sockfd;
    public:
        socket();
        socket(int fd);
        socket(int domain, int type);
        ~socket();
        socket(const socket&) = delete;
        socket(socket&& other) noexcept;

        auto operator=(const socket&) -> socket& = delete;
        auto operator=(socket&& other) noexcept -> socket&;

        auto close() -> void;
        auto fd() const -> int;
        auto receive() const -> std::string;
        auto send(const std::string& data) const -> void;
        auto valid() const -> bool;
    };

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream&;
}
