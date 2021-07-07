#include <netcore/event_monitor.h>
#include <netcore/server.h>
#include <netcore/signalfd.h>

#include <csignal>
#include <fcntl.h>
#include <ext/except.h>
#include <sys/un.h>
#include <timber/timber>
#include <utility>

namespace fs = std::filesystem;

namespace netcore {
    constexpr int stop_signals[] = {
        SIGINT,
        SIGTERM
    };

    server::server(const connection_handler on_connection) :
        on_connection(on_connection)
    {}

    server::server(server&& other) noexcept :
        sock(std::exchange(other.sock, socket()))
    {}

    auto server::accept() const -> int {
        auto client_addr = sockaddr_storage();
        socklen_t addrlen = sizeof(client_addr);

        auto client = ::accept(sock, (sockaddr*) &client_addr, &addrlen);

        if (client == -1) {
            throw ext::system_error("Failed to accpet client connection");
        }

        auto client_flags = fcntl(client, F_GETFL, 0);
        if (client_flags == -1) {
            throw ext::system_error("Failed to get client flags");
        }

        return client;
    }

    auto server::listen(
        int backlog,
        const std::function<void()>& callback
    ) const -> void {
        if (::listen(sock, backlog) == -1) {
            throw ext::system_error("Socket listen failure");
        }

        DEBUG() << sock << " listening for connections";

        // Leave room for:
        //      1. a full backlog of clients;
        //      2. the server socket;
        //      3. the signal socket.
        auto monitor = event_monitor(backlog + 2);

        monitor.set(EPOLLIN); // Event types for server socket.
        monitor.add(sock);

        // Event types for client and signal sockets.
        monitor.set(EPOLLIN | EPOLLRDHUP);

        const auto sigfd = signalfd::create(std::span(stop_signals));
        monitor.add(sigfd);

        callback();

        auto signal = 0;

        do {
            monitor.wait([&](const epoll_event& event) {
                auto current = event.data.fd;

                if (event.events & EPOLLRDHUP) {
                    // We need to call `close()` on this file descriptor.
                    // Create a socket instance, which will close
                    // the connection upon destruction.
                    auto s = socket(current);

                    DEBUG() << s << " peer closed connection, "
                        << "or shut down writing half of connection";

                    return;
                }

                if (current == static_cast<int>(sock)) monitor.add(accept());
                else if (current == static_cast<int>(sigfd)) {
                    signal = sigfd.signal();
                }
                else on_connection(current);
            });
        } while (!signal);

        INFO() << "signal (" << signal << "): " << strsignal(signal);
    }

    auto server::listen(
        const unix_socket& un,
        const std::function<void()>& callback,
        int backlog
    ) -> void {
        sock = socket(AF_UNIX, SOCK_STREAM);

        auto server_address = sockaddr_un();
        server_address.sun_family = AF_UNIX;
        auto string = un.path.string();
        string.copy(server_address.sun_path, string.size(), 0);

        // Remove the socket file if it exists.
        // It may be left over from a previous run where the program crashed.
        // If this file exists, the 'bind' operation will fail.
        if (fs::remove(un.path)) {
            WARNING() << "Removed existing socket file: " << un.path;
        }

        // Assign a socket file to the server socket.
        // This creates the file.
        if (bind(
            sock,
            (sockaddr*) &server_address,
            sizeof(sockaddr_un)
        ) == -1) {
            throw ext::system_error("Failed to bind socket to path: " + string);
        }
        DEBUG() << sock << " bound to path: " << un.path;

        un.apply_permissions();

        // Handle connections.
        listen(backlog, callback);

        if (fs::remove(un.path)) {
            DEBUG() << "Removed socket file: " << un.path;
        }
    }
}
