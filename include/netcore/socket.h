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
        auto end() const -> void;
        auto fd() const -> int;
        auto recv() const -> std::string;
        auto recv(void* buffer, std::size_t len) const -> std::size_t;
        auto send(const void* data, std::size_t len) const -> void;
        auto send(std::string_view string) const -> void;
        auto valid() const -> bool;
    };

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream&;
}
