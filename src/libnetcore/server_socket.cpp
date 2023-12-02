#include <netcore/server_socket.hpp>

#include <sys/socket.h>
#include <sys/un.h>

namespace netcore {
    server_socket::server_socket(int domain, int type, int protocol) :
        descriptor(
            ::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol)
        ),
        event(runtime::event::create(descriptor, EPOLLIN)) {
        if (!descriptor.valid()) {
            throw ext::system_error("Failed to create server socket");
        }
    }

    auto server_socket::accept() -> ext::task<socket> {
        auto client_addr = sockaddr_storage();
        socklen_t addrlen = sizeof(client_addr);

        while (true) {
            const auto client = ::accept4(
                descriptor,
                (sockaddr*) &client_addr,
                &addrlen,
                SOCK_NONBLOCK | SOCK_CLOEXEC
            );

            if (client != -1) co_return socket(client);

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (!co_await event->in()) co_return socket();
            }
            else
                throw ext::system_error(fmt::format(
                    "Failed to accept client connection on {}",
                    addr
                ));
        }
    }

    auto server_socket::address() const noexcept -> const address_type& {
        return addr;
    }

    auto server_socket::bind(const netcore::address& address) -> void {
        auto addr = socket_addr(address->ai_addr, address->ai_addrlen);

        int yes = 1;

        if (setsockopt(
                descriptor,
                SOL_SOCKET,
                SO_REUSEADDR,
                &yes,
                sizeof(yes)
            ) == -1)
            throw ext::system_error("Failed to set socket option");

        if (::bind(descriptor, address->ai_addr, address->ai_addrlen) == -1) {
            throw ext::system_error(fmt::format(
                "Failed to bind socket to {}:{}",
                addr.host(),
                addr.port()
            ));
        }

        TIMBER_DEBUG("{} bound to {}", *this, addr);

        this->addr = std::move(addr);
    }

    auto server_socket::bind(const std::filesystem::path& path) -> void {
        addr = path;

        auto address = sockaddr_un();
        address.sun_family = AF_UNIX;

        const auto string = path.string();
        string.copy(address.sun_path, string.size(), 0);

        if (::bind(descriptor, (sockaddr*) &address, sizeof(address)) == -1) {
            throw ext::system_error(
                fmt::format(R"(Failed to bind socket to path "{}")", string)
            );
        }

        TIMBER_DEBUG(R"({} bound to path "{}")", *this, string);
    }

    auto server_socket::cancel() -> void { event->cancel(); }

    auto server_socket::fd() const noexcept -> int { return descriptor; }

    auto server_socket::listen(int backlog) -> void {
        if (::listen(descriptor, backlog) == -1) {
            throw ext::system_error("Failed to listen for connections");
        }

        TIMBER_DEBUG(
            "{} listening for connections with a backlog size of {:L}",
            *this,
            backlog
        );
    }
}
