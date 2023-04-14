#pragma once

#include <array>
#include <netdb.h>
#include <filesystem>
#include <fmt/core.h>
#include <string_view>
#include <variant>

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

    enum class ip_addr {
        ipv4,
        ipv6
    };

    class socket_addr {
        std::string host_;
        unsigned short port_;
        ip_addr family_;
    public:
        socket_addr(sockaddr* addr, unsigned int addrlen);

        auto family() const noexcept -> ip_addr;

        auto host() const noexcept -> std::string_view;

        auto port() const noexcept -> unsigned short;
    };

    using address_type =
        std::variant<std::monostate, std::filesystem::path, socket_addr>;
}

template <>
struct fmt::formatter<netcore::address_type> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::address_type& addr, FormatContext& ctx) {
        return std::visit([&ctx](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                return format_to(ctx.out(), "<no address>");
            }
            else if constexpr (std::is_same_v<T, std::filesystem::path>) {
                return format_to(ctx.out(), R"("{}")", arg.native());
            }
            else return format_to(ctx.out(), "{}", arg);
        }, addr);
    }
};

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
