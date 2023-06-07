#pragma once

#include "context.hpp"
#include "socket.hpp"

#include <netcore/address.hpp>
#include <netcore/server.hpp>

namespace netcore::ssl {
    template <typename T>
    concept server_context = requires(
        T& t,
        socket&& client,
        const address_type& addr
    ) {
        { t.close() } -> std::same_as<void>;

        { t.connection(std::forward<socket>(client)) } ->
            std::same_as<ext::task<>>;

        { t.listen(addr) } -> std::same_as<void>;
    };

    template <server_context T>
    class server {
        T inner;
        context ctx;
    public:
        template <typename... Args>
        server(const context& ctx, Args&&... args) :
            inner(std::forward<Args>(args)...),
            ctx(ctx)
        {}

        auto close() -> void {
            inner.close();
        }

        auto connection(netcore::socket&& client) -> ext::task<> {
            co_await inner.connection(
                ctx.wrap(std::forward<netcore::socket>(client))
            );
        }

        auto listen(const address_type& addr) -> void {
            inner.listen(addr);
        }
    };
}
