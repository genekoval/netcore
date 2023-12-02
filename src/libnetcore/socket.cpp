#include <netcore/except.hpp>
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
    socket::socket(int fd) :
        descriptor(fd),
        event(runtime::event::create(fd, EPOLLIN | EPOLLOUT)) {
        TIMBER_TRACE("{} created", *this);
    }

    socket::socket(int domain, int type, int protocol) :
        socket(::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol)
        ) {
        if (!descriptor.valid()) {
            throw ext::system_error("Failed to create socket");
        }
    }

    auto socket::await_read() -> ext::task<> {
        if (!co_await event->in()) throw task_canceled();
    }

    auto socket::await_write() -> ext::task<> {
        if (!co_await event->out()) throw task_canceled();
    }

    auto socket::cancel() noexcept -> void { event->cancel(); }

    auto socket::connect(const sockaddr* addr, socklen_t len)
        -> ext::task<bool> {
        while (true) {
            if (::connect(descriptor, addr, len) == 0) co_return true;
            if (errno == EINPROGRESS) {
                if (!co_await event->out()) throw task_canceled();
            }
            else co_return false;
        }
    }

    auto socket::end() const -> void {
        if (::shutdown(descriptor, SHUT_WR) == -1) {
            throw ext::system_error("failed to shutdown further transmissions");
        }

        TIMBER_DEBUG("{} shutdown transmissions", *this);
    }

    auto socket::failed() const noexcept -> bool { return error; }

    auto socket::failure(const char* message) -> void {
        error = true;
        TIMBER_DEBUG("{} {}", *this, message);
        throw ext::system_error(message);
    }

    auto socket::fd() const noexcept -> int { return descriptor; }

    auto socket::read(void* dest, std::size_t len) -> ext::task<std::size_t> {
        auto bytes_read = -1;

        do {
            bytes_read = try_read(dest, len);
            if (bytes_read == -1) co_await await_read();
        } while (bytes_read == -1);

        co_return bytes_read;
    }

    auto socket::release()
        -> std::pair<netcore::fd, std::shared_ptr<runtime::event>> {
        return {std::move(descriptor), std::move(event)};
    }

    auto socket::sendfile(const netcore::fd& descriptor, std::size_t count)
        -> ext::task<> {
        auto sent = std::size_t();
        auto offset = off_t();

        while (sent < count) {
            const auto bytes =
                ::sendfile(this->descriptor, descriptor, &offset, count - sent);

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (!co_await event->out()) throw task_canceled();
                    continue;
                }

                failure("sendfile failure");
            }

            sent += bytes;

            TIMBER_DEBUG(
                "{} send {} bytes (sendfile [{}/{}])",
                *this,
                bytes,
                sent,
                count
            );
        }
    }

    auto socket::try_read(void* dest, std::size_t len) -> long {
        const auto bytes_read = ::recv(descriptor, dest, len, 0);

        if (bytes_read >= 0) {
            TIMBER_TRACE(
                "{} recv {:L} byte{}",
                *this,
                bytes_read,
                bytes_read == 1 ? "" : "s"
            );
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            failure("failed to receive data");
        }

        return bytes_read;
    }

    auto socket::try_write(const void* src, std::size_t len) -> long {
        const auto bytes_written = ::send(descriptor, src, len, MSG_NOSIGNAL);

        if (bytes_written >= 0) {
            TIMBER_TRACE(
                "{} send {:L} byte{}",
                *this,
                bytes_written,
                bytes_written == 1 ? "" : "s"
            );
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            failure("failed to send data");
        }

        return bytes_written;
    }

    auto socket::valid() const -> bool { return descriptor.valid(); }

    auto socket::write(const void* src, std::size_t len)
        -> ext::task<std::size_t> {
        auto bytes_written = -1;

        do {
            bytes_written = try_write(src, len);
            if (bytes_written == -1) co_await await_write();
        } while (bytes_written == -1);

        co_return bytes_written;
    }
}
