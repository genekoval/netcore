#pragma once

#include <netcore/address.hpp>
#include <netcore/endpoint.hpp>
#include <netcore/event.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>
#include <netcore/socket.h>

#include <ext/except.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    template <typename T>
    concept server_context = requires(T& t, socket&& client) {
        { t.connection(std::forward<socket>(client)) } ->
            std::same_as<ext::task<>>;
    };

    template <typename T>
    concept server_context_backlog = requires(T t) {
        { t.backlog } -> std::convertible_to<int>;
    };

    template <typename T>
    concept server_context_close = requires(T t) {
        { t.close() } -> std::same_as<void>;
    };

    template <typename T>
    concept server_context_listen = requires(T t, const address_type& addr) {
        { t.listen(addr) } -> std::same_as<void>;
    };

    template <typename T>
    concept server_context_shutdown = requires(T t) {
        { t.shutdown() } -> std::same_as<void>;
    };

    template <server_context T>
    class server final {
        class guard final {
            server* srv;
        public:
            guard(server* srv) : srv(srv) {}

            ~guard() {
                srv->reset();
            }
        };

        friend class guard;

        ext::counter connection_counter;
        bool close_requested = false;
        netcore::socket* socket = nullptr;
        address_type addr;

        auto accept() const -> ext::task<int> {
            auto client_addr = sockaddr_storage();
            socklen_t addrlen = sizeof(client_addr);

            while (true) {
                const auto client = ::accept4(
                    *socket,
                    (sockaddr*) &client_addr,
                    &addrlen,
                    SOCK_NONBLOCK | SOCK_CLOEXEC
                );

                if (client != -1) co_return client;

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await socket->wait();
                    if (close_requested) co_return -1;
                }
                else {
                    throw ext::system_error(
                        "failed to accept client connection"
                    );
                }
            };
        }

        auto handle_connection(int client) -> ext::detached_task {
            const auto counter_guard = connection_counter.increment();

            TIMBER_DEBUG("Client ({}) connected", client);

            try {
                co_await context.connection(netcore::socket(client, EPOLLIN));
            }
            catch (const std::exception& ex) {
                TIMBER_ERROR("Client connection closed: {}", ex.what());
            }
            catch (...) {
                TIMBER_ERROR("Client connection closed: Unknown error");
            }
        }

        auto listen() -> ext::task<> {
            auto backlog = SOMAXCONN;
            if constexpr (server_context_backlog<T>) backlog = context.backlog;

            close_requested = false;

            if (::listen(*socket, backlog) == -1) {
                throw ext::system_error("socket listen failure");
            }

            TIMBER_DEBUG("{} listening for connections", *socket);

            if constexpr (server_context_listen<T>) context.listen(addr);

            while (true) {
                int client = -1;

                try {
                    client = co_await accept();
                }
                catch (const task_canceled&) {
                    break;
                }
                catch (const ext::system_error& ex) {
                    TIMBER_ERROR(ex.what());
                }

                if (client == -1) break;
                handle_connection(client);
            }

            socket = nullptr;
            if constexpr (server_context_shutdown<T>) context.shutdown();
        }

        auto listen_priv(const inet_socket& inet) -> ext::task<> {
            auto addr = netcore::address(inet.host, inet.port);

            auto sock = netcore::socket(
                addr->ai_family,
                addr->ai_socktype,
                addr->ai_protocol,
                EPOLLIN
            );
            socket = &sock;
            this->addr = socket_addr(addr->ai_addr, addr->ai_addrlen);

            {
                int yes = 1;
                if (setsockopt(
                    sock,
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &yes,
                    sizeof(yes)
                ) == -1) throw ext::system_error("Failed to set socket option");
            }

            if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
                throw ext::system_error(fmt::format(
                    "Failed to bind socket to {}:{}",
                    inet.host,
                    inet.port
                ));
            }

            TIMBER_DEBUG(
                "{} bound to {}",
                sock,
                std::get<socket_addr>(this->addr)
            );

            co_await listen();
        }

        auto listen_priv(const unix_socket& unix_socket) -> ext::task<> {
            auto sock = netcore::socket(AF_UNIX, SOCK_STREAM, 0, EPOLLIN);
            socket = &sock;
            addr = unix_socket.path;

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

            TIMBER_DEBUG(R"({} bound to path: "{}")", sock, string);

            unix_socket.apply_permissions();

            co_await listen();

            if (std::filesystem::remove(unix_socket.path)) {
                TIMBER_DEBUG(R"(Removed socket file: "{}")", string);
            }
        }

        auto reset() noexcept -> void {
            addr = std::monostate();
            close_requested = false;
            socket = nullptr;
        }

        auto wait_for_connections() -> ext::task<> {
            co_await connection_counter.await();
            if constexpr (server_context_close<T>) context.close();
        }
    public:
        T context;

        template <typename... Args>
        server(Args&&... args) : context(std::forward<Args>(args)...) {}

        server(const server& other) = delete;

        server(server&& other) = delete;

        auto operator=(const server& other) -> server& = delete;

        auto operator=(server&& other) -> server& = delete;

        auto address() const noexcept -> const address_type& {
            return addr;
        }

        auto close() noexcept -> void {
            if (listening()) {
                close_requested = true;
                socket->notify();
            }

            if (connection_counter) {
                TIMBER_INFO(
                    "Waiting for {:L} connection{} on {}",
                    connection_counter.count(),
                    connection_counter.count() == 1 ? "" : "s",
                    addr
                );
            }
        }

        auto connections() const noexcept -> unsigned int {
            return connection_counter.count();
        }

        auto listen(const endpoint& endpoint) -> ext::jtask<> {
            const auto guard = server::guard(this);

            co_await std::visit([&](auto&& arg) {
                return listen_priv(arg);
            }, endpoint);

            co_await wait_for_connections();
        }

        auto listening() const noexcept -> bool {
            return socket != nullptr;
        }
    };
}
