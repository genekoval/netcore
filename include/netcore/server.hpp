#pragma once

#include "server_socket.hpp"

#include <netcore/address.hpp>
#include <netcore/endpoint.hpp>
#include <netcore/event.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>

#include <ext/except.h>
#include <ext/scope>
#include <fcntl.h>
#include <filesystem>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    template <typename T>
    concept server_context = requires(T& t, socket&& client) {
        {
            t.connection(std::forward<socket>(client))
        } -> std::same_as<ext::task<>>;
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
        ext::counter connection_counter;
        server_socket* socket = nullptr;
        address_type addr;

        auto handle_connection(netcore::socket&& client) -> ext::detached_task {
            const auto fd = client.fd();
            const auto counter_guard = connection_counter.increment();

            TIMBER_DEBUG(
                "Client ({}) connected: {:L} total",
                fd,
                connection_counter.count()
            );

            try {
                co_await context.connection(std::move(client));
            }
            catch (const std::exception& ex) {
                TIMBER_ERROR("Client connection closed: {}", ex.what());
            }
            catch (...) {
                TIMBER_ERROR("Client connection closed: Unknown error");
            }

            TIMBER_DEBUG(
                "Client ({}) disconnected: {:L} total",
                fd,
                connection_counter.count() - 1
            );
        }

        auto listen(server_socket socket) -> ext::task<> {
            auto backlog = SOMAXCONN;
            if constexpr (server_context_backlog<T>) backlog = context.backlog;

            socket.listen(backlog);
            if constexpr (server_context_listen<T>) {
                context.listen(socket.address());
            }

            this->socket = &socket;
            addr = socket.address();
            const auto deferred =
                ext::scope_exit([this] { this->socket = nullptr; });

            while (true) {
                try {
                    auto client = co_await socket.accept();

                    if (!client.valid()) break;

                    handle_connection(std::move(client));
                }
                catch (const ext::system_error& ex) {
                    switch (ex.code().value()) {
                        case ECONNABORTED:
                        case EPERM: TIMBER_ERROR(ex.what());
                        default: throw;
                    }
                }
            }
        }

        auto listen_priv(const inet_socket& inet) -> ext::task<> {
            auto addr = netcore::address(inet.host, inet.port);

            auto socket = server_socket(
                addr->ai_family,
                addr->ai_socktype,
                addr->ai_protocol
            );

            socket.bind(addr);

            co_await listen(std::move(socket));
        }

        auto listen_priv(const unix_socket& unix_socket) -> ext::task<> {
            auto socket = server_socket(AF_UNIX, SOCK_STREAM, 0);
            socket.bind(unix_socket.path);

            unix_socket.apply_permissions();

            const auto deferred =
                ext::scope_exit([&] { unix_socket.remove(); });

            co_await listen(std::move(socket));
        }
    public:
        T context;

        template <typename... Args>
        server(Args&&... args) : context(std::forward<Args>(args)...) {}

        server(const server& other) = delete;

        server(server&& other) = delete;

        auto operator=(const server& other) -> server& = delete;

        auto operator=(server&& other) -> server& = delete;

        auto address() const noexcept -> const address_type& { return addr; }

        auto close() noexcept -> void {
            if (socket) socket->cancel();

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
            const auto deferred =
                ext::scope_exit([this] { addr = std::monostate(); });

            co_await std::visit(
                [&](auto&& arg) { return listen_priv(arg); },
                endpoint
            );

            if constexpr (server_context_shutdown<T>) context.shutdown();

            if (connection_counter) co_await connection_counter.await();
            else co_await yield();

            if constexpr (server_context_close<T>) context.close();
        }

        auto listening() const noexcept -> bool { return socket != nullptr; }
    };
}
