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
    server::server(const connection_handler on_connection) :
        on_connection(on_connection)
    {}

    server::server(server&& other) noexcept :
        sock(std::exchange(other.sock, socket()))
    {}

    auto server::accept() const -> int {
        auto client_addr = sockaddr_storage();
        socklen_t addrlen = sizeof(client_addr);

        auto client = ::accept(sock.fd(), (sockaddr*) &client_addr, &addrlen);

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
        if (::listen(sock.fd(), backlog) == -1) {
            throw ext::system_error("Socket listen failure");
        }

        DEBUG() << sock << " listening for connections";
        callback();

        // Leave room for:
        //      1. a full backlog of clients;
        //      2. the server socket;
        //      3. the signal socket.
        auto monitor = event_monitor(backlog + 2);

        monitor.set(EPOLLIN); // Event types for server socket.
        monitor.add(sock);

        // Event types for client and signal sockets.
        monitor.set(EPOLLIN | EPOLLRDHUP);

        const auto sigfd = signalfd::create({SIGINT, SIGTERM});
        monitor.add(sigfd.fd());

        auto signal = 0;

        do {
            monitor.wait([&](const epoll_event& event) {
                if (event.events & EPOLLRDHUP) {
                    WARN() << "Client disconnect";
                    return;
                }

                auto current = event.data.fd;

                if (current == sock.fd()) monitor.add(accept());
                else if (current == sigfd.fd()) signal = sigfd.signal();
                else on_connection(socket(current));
            });
        } while (!signal);

        INFO() << "signal (" << signal << "): " << strsignal(signal);
    }

    auto server::listen(
        const fs::path& path,
        const std::function<void()>& callback,
        int backlog
    ) -> void {
        listen(path, fs::perms::unknown, callback, backlog);
    }

    auto server::listen(
        const fs::path& path,
        fs::perms perms,
        const std::function<void()>& callback,
        int backlog
    ) -> void {
        sock = socket(AF_UNIX, SOCK_STREAM);

        auto server_address = sockaddr_un();
        server_address.sun_family = AF_UNIX;
        auto string = path.string();
        string.copy(server_address.sun_path, string.size(), 0);

        // Remove the socket file if it exists.
        // It may be left over from a previous run where the program crashed.
        // If this file exists, the 'bind' operation will fail.
        if (fs::remove(path)) {
            WARN() << "Removed existing socket file: " << path;
        }

        // Assign a socket file to the server socket.
        // This creates the file.
        if (bind(
            sock.fd(),
            (sockaddr*) &server_address,
            sizeof(sockaddr_un)
        ) == -1) {
            throw ext::system_error("Failed to bind socket to path: " + string);
        }
        DEBUG() << sock << " bound to path: " << path;

        // Set socket file permissions.
        if (perms != fs::perms::unknown) fs::permissions(path, perms);

        // Handle connections.
        listen(backlog, callback);

        // The server is shutting down. Clean up.
        fs::remove(path);
    }
}
