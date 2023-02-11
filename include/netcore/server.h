#pragma once

#include <netcore/event.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>
#include <netcore/socket.h>
#include <netcore/unix_socket.h>

#include <ext/except.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    template <typename T>
    concept server_context = requires(T& t, socket&& client) {
        { t.close() } -> std::same_as<void>;

        { t.connection(std::forward<socket>(client)) } ->
            std::same_as<ext::task<>>;

        { t.listen() } -> std::same_as<void>;
    };

    template <server_context T>
    class server {
        event<> no_connections;
        unsigned int connection_count = 0;
        bool close_requested = false;
        socket* socket_ptr = nullptr;

        auto accept(socket& sock) const -> ext::task<int> {
            auto client_addr = sockaddr_storage();
            socklen_t addrlen = sizeof(client_addr);

            auto client = -1;

            do {
                client = ::accept4(
                    sock,
                    (sockaddr*) &client_addr,
                    &addrlen,
                    SOCK_NONBLOCK | SOCK_CLOEXEC
                );

                if (client == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        co_await sock.wait();

                        const auto shutting_down =
                            runtime::current().shutting_down();

                        if (shutting_down || close_requested) break;
                    }
                    else {
                        throw ext::system_error(
                            "failed to accept client connection"
                        );
                    }
                }
            } while (client == -1);

            co_return client;
        }

        auto handle_connection(int client) -> ext::detached_task {
            ++connection_count;

            TIMBER_DEBUG("client ({}) connected", client);

            try {
                co_await context.connection(socket(client, EPOLLIN));
            }
            catch (const std::exception& ex) {
                TIMBER_ERROR("client connection closed: {}", ex.what());
            }
            catch (...) {
                TIMBER_ERROR("client connection closed: unknown error");
            }

            --connection_count;
            if (connection_count == 0) no_connections.emit();
        }

        auto listen(socket& sock, int backlog) -> ext::task<> {
            close_requested = false;

            if (::listen(sock, backlog) == -1) {
                throw ext::system_error("socket listen failure");
            }

            TIMBER_DEBUG("{} listening for connections", sock);
            socket_ptr = &sock;
            context.listen();

            while (true) {
                try {
                    const auto client = co_await accept(sock);
                    if (client == -1) break;
                    handle_connection(client);
                }
                catch (const task_canceled&) {
                    break;
                }
                catch (const ext::system_error& ex) {
                    TIMBER_ERROR(ex.what());
                }
            }

            socket_ptr = nullptr;
        }

        auto listen_priv(
            const unix_socket& unix_socket,
            int backlog
        ) -> ext::task<> {
            auto sock = socket(AF_UNIX, SOCK_STREAM, EPOLLIN);

            auto server_address = sockaddr_un();
            server_address.sun_family = AF_UNIX;
            const auto string = unix_socket.path.string();
            string.copy(server_address.sun_path, string.size(), 0);

            if (bind(
                sock,
                (sockaddr*) &server_address,
                sizeof(sockaddr_un)
            ) == -1) {
                throw ext::system_error(fmt::format(
                    "Failed to bind socket to path: {}",
                    string
                ));
            }

            TIMBER_DEBUG("{} bound to path: {}", sock, string);

            unix_socket.apply_permissions();

            co_await listen(sock, backlog);

            if (std::filesystem::remove(unix_socket.path)) {
                TIMBER_DEBUG("Removed socket file: {}", string);
            }
        }

        auto wait_for_connections() -> ext::task<> {
            if (connection_count > 0) {
                TIMBER_INFO(
                    "Waiting for {:L} connection{}",
                    connection_count,
                    connection_count == 1 ? "" : "s"
                );

                co_await no_connections.listen();
            }

            context.close();
        }
    public:
        T context;

        template <typename... Args>
        server(Args&&... args) : context(std::forward<Args>(args)...) {}

        server(server&& other) = delete;

        auto close() noexcept -> void {
            close_requested = true;
            if (socket_ptr) socket_ptr->notify();
        }

        auto connections() const noexcept -> unsigned int {
            return connection_count;
        }

        auto listen(
            const unix_socket& unix_socket,
            int backlog = SOMAXCONN
        ) -> ext::task<> {
            co_await listen_priv(unix_socket, backlog);
            co_await wait_for_connections();
        }

        auto listening() const noexcept -> bool {
            return socket_ptr;
        }
    };
}
