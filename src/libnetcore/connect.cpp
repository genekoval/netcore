#include <netcore/address.hpp>
#include <netcore/connect.hpp>

#include <cstring>
#include <ext/except.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    auto connect(
        std::string_view host,
        std::string_view port
    ) -> ext::task<socket> {
        auto addr = address(host, port);

        for (auto* res = addr.get(); res != nullptr; res = res->ai_next) {
            auto sock = socket(
                res->ai_family,
                res->ai_socktype,
                res->ai_protocol
            );

            if (co_await sock.connect(res->ai_addr, res->ai_addrlen)) {
                TIMBER_DEBUG(
                    "{} connected to {}",
                    sock,
                    socket_addr(res->ai_addr, res->ai_addrlen)
                );

                co_return sock;
            }
        }

        throw ext::system_error(fmt::format(
            "Failed to connect to ({}:{})", host, port
        ));
    }

    auto connect(std::string_view path) -> ext::task<socket> {
        auto addr = sockaddr_un();
        addr.sun_family = AF_UNIX;
        path.copy(addr.sun_path, path.size());

        auto sock = socket(AF_UNIX, SOCK_STREAM, 0);

        if (co_await sock.connect((sockaddr*) &addr, sizeof(addr))) {
            TIMBER_DEBUG("{} connected to \"{}\"", sock, path);
            co_return sock;
        }

        throw ext::system_error(fmt::format(
            "Failed to connect to \"{}\"",
            path
        ));
    }

    auto connect(const endpoint& endpoint) -> ext::task<socket> {
        return std::visit([](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, inet_socket>) {
                return connect(arg.host, arg.port);
            }

            if constexpr (std::is_same_v<T, unix_socket>) {
                return connect(arg.path.native());
            }
        }, endpoint);
    }
}
