#include <netcore/client.h>

#include <cstring>
#include <ext/except.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
    auto connect(std::string_view host, std::string_view port) -> socket {
        auto hints = addrinfo();
        addrinfo* result = nullptr;

        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        getaddrinfo(host.data(), port.data(), &hints, &result);

        auto sock = socket(result->ai_family, result->ai_socktype);

        if (::connect(sock.fd(), result->ai_addr, result->ai_addrlen) == -1) {
            throw ext::system_error(
                "failed to connect to: " +
                std::string(host) + ":" + std::string(port)
            );
        }

        DEBUG() << sock << " connected to: " << host << ':' << port;

        return sock;
    }

    auto connect(std::string_view path) -> socket {
        auto addr = sockaddr_un();
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        path.copy(addr.sun_path, path.size());

        auto sock = socket(AF_UNIX, SOCK_STREAM);

        if (::connect(sock.fd(), (sockaddr*) &addr, sizeof(addr)) == -1) {
            throw ext::system_error(
                "failed to connect to: " + std::string(path)
            );
        }

        DEBUG() << sock << " connected to: " << path;

        return sock;
    }
}
