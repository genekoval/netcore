#include <netcore/client.h>

#include <cstring>
#include <ext/except.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <timber/timber>

namespace netcore {
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

        INFO() << "Connected to: " << path;

        return sock;
    }
}
