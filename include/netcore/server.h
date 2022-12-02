#pragma once

#include <netcore/event.hpp>
#include <netcore/socket.h>
#include <netcore/unix_socket.h>

#include <filesystem>
#include <functional>
#include <sys/socket.h>

namespace netcore {
    using connection_handler = std::function<ext::task<>(socket&&)>;
    using listen_callback = std::function<void()>;

    class server {
        event<> close;
        unsigned int connection_count = 0;
        connection_handler on_connection;

        auto accept(socket& sock) const -> ext::task<int>;

        auto handle_connection(int client) -> ext::detached_task;

        auto listen(
            socket& sock,
            const listen_callback& callback,
            int backlog
        ) -> ext::task<>;

        auto listen_priv(
            const unix_socket& unix_socket,
            const listen_callback& callback,
            int backlog
        ) -> ext::task<>;

        auto wait_for_connections() -> ext::task<>;
    public:
        server(connection_handler&& on_connection);

        server(server&& other) = delete;

        auto connections() const noexcept -> unsigned int;

        auto listen(
            const unix_socket& unix_socket,
            const listen_callback& callback,
            int backlog = SOMAXCONN
        ) -> ext::task<>;
    };
}
