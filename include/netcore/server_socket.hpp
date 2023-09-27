#pragma once

#include "address.hpp"
#include "endpoint.hpp"
#include "fd.hpp"
#include "runtime.hpp"
#include "socket.h"

namespace netcore {
    class server_socket {
        netcore::fd descriptor;
        std::shared_ptr<runtime::event> event;
        address_type addr;
    public:
        server_socket(int domain, int type, int protocol);

        auto accept() -> ext::task<socket>;

        auto address() const noexcept -> const address_type&;

        auto bind(const netcore::address& address) -> void;

        auto bind(const std::filesystem::path& path) -> void;

        auto cancel() -> void;

        auto fd() const noexcept -> int;

        auto listen(int backlog) -> void;
    };
}

template <>
struct fmt::formatter<netcore::server_socket> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::server_socket& socket, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "server ({})", socket.fd());
    }
};
