#include <netcore/socket.h>

#include <cstring>
#include <ext/except.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <timber/timber>
#include <unistd.h>
#include <utility>

namespace netcore {
    socket::socket(int fd, uint32_t events) :
        descriptor(fd),
        event(fd, events)
    {
        TIMBER_TRACE("{} created", *this);
    }

    socket::socket(int domain, int type, int protocol, uint32_t events) :
        socket(::socket(
            domain,
            type | SOCK_NONBLOCK | SOCK_CLOEXEC,
            protocol
        ), events)
    {
        if (!descriptor.valid()) {
            throw ext::system_error("failed to create socket");
        }
    }

    socket::operator int() const { return descriptor; }

    auto socket::cancel() noexcept -> void {
        event.cancel();
    }

    auto socket::end() const -> void {
        if (::shutdown(descriptor, SHUT_WR) == -1) {
            throw ext::system_error("failed to shutdown further transmissions");
        }

        TIMBER_DEBUG("{} shutdown transmissions", *this);
    }

    auto socket::failed() const noexcept -> bool {
        return error;
    }

    auto socket::failure(const char* message) -> void {
        error = true;
        TIMBER_DEBUG("{} {}", *this, message);
        throw ext::system_error(message);
    }

    auto socket::notify() -> void {
        event.notify();
    }

    auto socket::read(
        void* dest,
        std::size_t len
    ) -> ext::task<std::size_t> {
        auto bytes_read = -1;

        do {
            bytes_read = try_read(dest, len);
            if (bytes_read == -1) co_await wait(EPOLLIN);
        } while (bytes_read == -1);

        TIMBER_TRACE(
            "{} recv {:L} byte{}",
            *this,
            bytes_read,
            bytes_read == 1 ? "" : "s"
        );

        co_return bytes_read;
    }

    auto socket::sendfile(
        const fd& descriptor,
        std::size_t count
    ) -> ext::task<> {
        auto sent = std::size_t();
        auto offset = off_t();

        while (sent < count) {
            const auto bytes = ::sendfile(
                this->descriptor,
                descriptor,
                &offset,
                count - sent
            );

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await wait(EPOLLOUT);
                    continue;
                }

                failure("sendfile failure");
            }

            sent += bytes;

            TIMBER_DEBUG(
                "{} send {} bytes (sendfile [{}/{}])", *this, bytes, sent, count
            );
        }
    }

    auto socket::try_read(void* dest, std::size_t len) -> long {
        const auto bytes_read = ::recv(descriptor, dest, len, 0);

        if (bytes_read == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
            failure("failed to receive data");
        }

        return bytes_read;
    }

    auto socket::valid() const -> bool { return descriptor.valid(); }

    auto socket::wait(uint32_t events) -> ext::task<> {
        co_await event.wait(events, nullptr);
    }

    auto socket::write(
        const void* src,
        std::size_t len
    ) -> ext::task<std::size_t> {
        auto bytes_written = -1;

        do {
            bytes_written = ::send(descriptor, src, len, MSG_NOSIGNAL);

            if (bytes_written == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await wait(EPOLLOUT);
                    continue;
                }

                failure("failed to send data");
            }
        } while (bytes_written == -1);

        TIMBER_TRACE(
            "{} send {:L} byte{}",
            *this,
            bytes_written,
            bytes_written == 1 ? "" : "s"
        );

        co_return bytes_written;
    }
}
