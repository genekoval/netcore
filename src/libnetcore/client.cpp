#include "address.h"

#include <netcore/client.h>

#include <cstring>
#include <ext/except.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    auto connect(
        std::string_view host,
        std::string_view port
    ) -> ext::task<socket> {
        auto addr = address(host, port);

        for (auto* res = addr.data(); res != nullptr; res = res->ai_next) {
            auto sock = socket(res->ai_family, res->ai_socktype, EPOLLOUT);

            while (true) {
                if (::connect(sock, res->ai_addr, res->ai_addrlen) == 0) {
                    TIMBER_DEBUG("{} connected to {}:{}", sock, host, port);
                    co_return sock;
                }

                if (errno != EINPROGRESS) break;

                co_await sock.wait();
            }
        }

        throw ext::system_error(fmt::format(
            "Failed to connect to ({}:{})", host, port
        ));
    }

    auto connect(std::string_view path) -> ext::task<socket> {
        auto addr = sockaddr_un();
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        path.copy(addr.sun_path, path.size());

        auto sock = socket(AF_UNIX, SOCK_STREAM, EPOLLOUT);

        while (true) {
            if (::connect(sock, (sockaddr*) &addr, sizeof(addr)) == 0) {
                TIMBER_DEBUG("{} connected to \"{}\"", sock, path);
                co_return sock;
            }

            if (errno != EINPROGRESS) break;

            co_await sock.wait();
        }

        throw ext::system_error(fmt::format(
            "Failed to connect to \"{}\"",
            path
        ));
    }
}
