#include <netcore/socket.h>

#include <cstring>
#include <ext/except.h>
#include <iostream>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <timber/timber>
#include <unistd.h>
#include <utility>

namespace netcore {
    socket::socket(int fd) : sockfd(fd) {
        DEBUG() << *this << " created";
    }

    socket::socket(int domain, int type) : socket(::socket(domain, type, 0)) {
        if (!sockfd.valid()) throw ext::system_error("Failed to create socket");
    }

    socket::operator int() const { return sockfd; }

    auto socket::end() const -> void {
        if (shutdown(sockfd, SHUT_WR) == -1) {
            throw ext::system_error("failed to shutdown further transmissions");
        }

        DEBUG() << *this << " shutdown transmissions";
    }

    auto socket::read(void* buffer, std::size_t len) const -> std::size_t {
        auto bytes = ::recv(sockfd, buffer, len, 0);

        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            throw ext::system_error("failed to receive data");
        }

        DEBUG() << *this << " recv " << bytes << " bytes";

        return bytes;
    }

    auto socket::sendfile(const fd& descriptor, std::size_t count) const -> void {
        auto sent = std::size_t();
        auto offset = off_t();

        while (sent < count) {
            const auto bytes = ::sendfile(
                sockfd,
                descriptor,
                &offset,
                count - sent
            );

            if (bytes == -1) throw ext::system_error("sendfile failure");

            sent += bytes;

            DEBUG()
                << *this << " send " << bytes << " bytes (sendfile ["
                << sent << "/" << count << "])";
        }
    }

    auto socket::valid() const -> bool { return sockfd.valid(); }

    auto socket::write(const void* data, std::size_t len) const -> std::size_t {
        auto bytes = ::send(sockfd, data, len, MSG_NOSIGNAL);

        if (bytes == -1) {
            DEBUG() << *this << " failed to send data";
            throw ext::system_error("failed to send data");
        }

        DEBUG() << *this << " send " << bytes << " bytes";

        return bytes;
    }

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream& {
        os << "socket (" << static_cast<int>(sock) << ")";
        return os;
    }
}
