#include <netcore/except.hpp>
#include <netcore/runtime.hpp>
#include <netcore/server.h>
#include <netcore/signalfd.h>

#include <csignal>
#include <fcntl.h>
#include <ext/except.h>
#include <sys/un.h>
#include <timber/timber>

namespace fs = std::filesystem;

namespace netcore {
    server::server(connection_handler&& on_connection) :
        on_connection(std::move(on_connection))
    {}

    auto server::accept(socket& sock) const -> ext::task<int> {
        auto client_addr = sockaddr_storage();
        socklen_t addrlen = sizeof(client_addr);

        auto client = -1;

        do {
            client = ::accept4(
                sock,
                (sockaddr*) &client_addr,
                &addrlen,
                SOCK_NONBLOCK
            );

            if (client == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await sock.wait();
                    if (runtime::current().shutting_down()) break;
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

    auto server::connections() const noexcept -> unsigned int {
        return connection_count;
    }

    auto server::handle_connection(int client) -> ext::detached_task {
        ++connection_count;

        TIMBER_DEBUG("client ({}) connected", client);

        try {
            co_await on_connection(socket(client, EPOLLIN));
        }
        catch (const std::exception& ex) {
            TIMBER_ERROR("client connection closed: {}", ex.what());
        }
        catch (...) {
            TIMBER_ERROR("client connection closed: unknown error");
        }

        --connection_count;
        if (connection_count == 0) close.emit();
    }

    auto server::listen(
        socket& sock,
        const listen_callback& callback,
        int backlog
    ) -> ext::task<> {
        if (::listen(sock, backlog) == -1) {
            throw ext::system_error("socket listen failure");
        }

        TIMBER_DEBUG("{} listening for connections", sock);
        callback();

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
    }

    auto server::listen(
        const unix_socket& unix_socket,
        const listen_callback& callback,
        int backlog
    ) -> ext::task<> {
        co_await listen_priv(unix_socket, callback, backlog);
        co_await wait_for_connections();
    }

    auto server::listen_priv(
        const unix_socket& unix_socket,
        const listen_callback& callback,
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
            throw ext::system_error("Failed to bind socket to path: " + string);
        }

        TIMBER_DEBUG("{} bound to path: {}", sock, string);

        unix_socket.apply_permissions();

        co_await listen(sock, callback, backlog);

        if (fs::remove(unix_socket.path)) {
            TIMBER_DEBUG("Removed socket file: {}", string);
        }
    }

    auto server::wait_for_connections() -> ext::task<> {
        if (connection_count == 0) co_return;

        TIMBER_INFO(
            "Waiting for {:L} connection{}",
            connection_count,
            connection_count == 1 ? "" : "s"
        );

        co_await close.listen();
    }
}
