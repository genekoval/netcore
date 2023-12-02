#pragma once

#include "context.hpp"
#include "socket.hpp"

#include <netcore/address.hpp>
#include <netcore/server.hpp>

namespace netcore::ssl {
    template <typename T>
    concept server_context = requires(T& t, socket&& client) {
        {
            t.connection(std::forward<socket>(client))
        } -> std::same_as<ext::task<>>;
    };

    template <server_context T>
    class server {
        T inner;
        context ctx;
    public:
        template <typename... Args>
        server(const context& ctx, Args&&... args) :
            inner(std::forward<Args>(args)...),
            ctx(ctx) {}

        auto close() -> void
        requires server_context_close<T>
        {
            inner.close();
        }

        auto connection(netcore::socket&& client) -> ext::task<> {
            co_await inner.connection(
                ctx.wrap(std::forward<netcore::socket>(client))
            );
        }

        auto listen(const address_type& addr) -> void
        requires server_context_listen<T>
        {
            inner.listen(addr);
        }

        auto shutdown() -> void
        requires server_context_shutdown<T>
        {
            inner.shutdown();
        }
    };
}
