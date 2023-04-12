#pragma once

#include <array>
#include <netdb.h>
#include <fmt/core.h>
#include <string_view>

namespace netcore {
    class address {
        addrinfo* info = nullptr;
    public:
        address() = default;

        address(std::string_view host, std::string_view port);

        ~address();

        auto operator*() const noexcept -> addrinfo&;

        auto operator->() const noexcept -> addrinfo*;

        auto get() -> addrinfo*;
    };

    class socket_addr {
        std::array<char, NI_MAXHOST> host_buffer;
        std::array<char, NI_MAXSERV> port_buffer;
    public:
        socket_addr(sockaddr* addr, unsigned int addrlen);

        auto host() const noexcept -> std::string_view;

        auto port() const noexcept -> std::string_view;
    };
}

template <>
struct fmt::formatter<netcore::socket_addr> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::socket_addr& addr, FormatContext& ctx) {
        return format_to(ctx.out(), "{}:{}", addr.host(), addr.port());
    }
};
