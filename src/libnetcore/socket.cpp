#include <netcore/socket.h>

#include <cstring>
#include <ext/except.h>
#include <iostream>
#include <sys/socket.h>
#include <timber/timber>
#include <unistd.h>
#include <utility>

constexpr auto recv_buffer_size = 256;
constexpr auto invalid_socket = -1;

namespace netcore {
    socket::socket() : sockfd(invalid_socket) {
        TRACE() << "Placeholder socket created";
    }

    socket::socket(int fd) : sockfd(fd) {
        TRACE() << *this << " created from existing file descriptor";
    }

    socket::socket(int domain, int type) :
        socket(::socket(domain, type, 0))
    {
        if (!valid()) throw ext::system_error("Failed to create socket");
        TRACE() << *this << " created";
    }

    socket::~socket() {
        TRACE() << *this << " destructor called";
        close();
    }

    socket::socket(socket&& other) noexcept :
        sockfd(std::exchange(other.sockfd, invalid_socket))
    {}

    auto socket::operator=(socket&& other) noexcept -> socket& {
        sockfd = std::exchange(other.sockfd, invalid_socket);
        return *this;
    }

    auto socket::close() -> void {
        if (!valid()) return;

        if (::close(sockfd) == -1) {
            ERROR() << *this << " failed to close: " << std::strerror(errno);
        }
        else {
            DEBUG() << *this << " closed";
        }

        sockfd = -1; // Invalidate this socket instance.
    }

    auto socket::fd() const -> int { return sockfd; }

    auto socket::recv() const -> std::string {
        auto data = std::string();
        char buffer[recv_buffer_size];
        auto byte_count = 0;

        do {
            byte_count = ::recv(sockfd, buffer, recv_buffer_size, 0);
            if (byte_count > 0) data.append(buffer, byte_count);
        } while (byte_count > 0);

        if (byte_count == -1 && errno != EAGAIN) {
            ERROR() << *this << " failed to read: " << std::strerror(errno);
        }

        return data;
    }

    auto socket::recv(void* buffer, std::size_t len) const -> std::size_t {
        auto bytes = ::recv(sockfd, buffer, len, 0);

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            throw ext::system_error("failed to receive data");
        }

        if (static_cast<std::size_t>(bytes) < len) {
            WARN()
                << *this << " recv " << bytes << " bytes; expected " << len;
        }

        DEBUG() << *this << " recv " << bytes << " bytes";

        return bytes;
    }

    auto socket::send(const void* data, std::size_t len) const -> void {
        auto bytes = ::send(sockfd, data, len, MSG_NOSIGNAL);

        if (bytes == -1) {
            throw ext::system_error("failed to send data");
        }

        if (static_cast<std::size_t>(bytes) < len) {
            WARN()
                << *this << " send " << bytes << " bytes; expected " << len;
        }

        DEBUG() << *this << " send " << bytes << " bytes";
    }

    auto socket::send(std::string_view string) const -> void {
        send(string.data(), string.size());
    }

    auto socket::valid() const -> bool { return sockfd != invalid_socket; }

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream& {
        os << "socket (" << sock.fd() << ")";
        return os;
    }
}
