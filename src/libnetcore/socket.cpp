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
        TRACE() << *this << " closing...";

        if (!valid()) return;

        if (::close(sockfd) == -1) {
            ERROR() << *this << " failed to close: " << std::strerror(errno);
        }

        sockfd = -1; // Invalidate this socket instance.
    }

    auto socket::fd() const -> int { return sockfd; }

    auto socket::receive() const -> std::string {
        auto data = std::string();
        char buffer[recv_buffer_size];
        auto byte_count = 0;

        do {
            byte_count = recv(sockfd, buffer, recv_buffer_size, 0);
            if (byte_count > 0) data.append(buffer, byte_count);
        } while (byte_count > 0);

        if (byte_count == -1 && errno != EAGAIN) {
            ERROR() << *this << " failed to read: " << std::strerror(errno);
        }

        return data;
    }

    auto socket::send(const std::string& data) const -> void {
        TRACE() << *this << " sending string data";
        DEBUG() << *this << " sending " << data.size() << " bytes";

        auto bytes_sent = ::send(
            sockfd,
            data.c_str(),
            data.size(),
            MSG_NOSIGNAL
        );

        if (bytes_sent == -1) {
            ERROR() << *this << " failed to send: " << std::strerror(errno);
        }
        else DEBUG() << *this << " sent " << bytes_sent << " bytes";
    }

    auto socket::valid() const -> bool { return sockfd != invalid_socket; }

    auto operator<<(std::ostream& os, const socket& sock) -> std::ostream& {
        os << "socket (" << sock.fd() << ")";
        return os;
    }
}
