#include "address.h"

#include <netcore/client.h>

#include <cstring>
#include <ext/except.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    auto connect(std::string_view host, std::string_view port) -> socket {
        auto addr = address(host, port);

        for (auto* res = addr.data(); res != nullptr; res = res->ai_next) {
            auto sock = socket(res->ai_family, res->ai_socktype);

            if (::connect(sock, res->ai_addr, res->ai_addrlen) != -1) {
                DEBUG() << sock << " connected to: " << host << ":" << port;
                return sock;
            }
        }

        throw ext::system_error(
            "failed to connect to: " +
            std::string(host) + ":" + std::string(port)
        );
    }

    auto connect(std::string_view path) -> socket {
        auto addr = sockaddr_un();
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        path.copy(addr.sun_path, path.size());

        auto sock = socket(AF_UNIX, SOCK_STREAM);

        if (::connect(sock, (sockaddr*) &addr, sizeof(addr)) != -1) {
            DEBUG() << sock << " connected to: " << path;
            return sock;
        }

        throw ext::system_error("failed to connect to: " + std::string(path));
    }
}
