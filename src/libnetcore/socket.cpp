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

    socket::socket(int domain, int type, uint32_t events) :
        socket(::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, 0), events)
    {
        if (!descriptor.valid()) {
            throw ext::system_error("failed to create socket");
        }
    }

    socket::operator int() const { return descriptor; }

    auto socket::cancel() noexcept -> void {
        event.cancel();
    }

    auto socket::deregister() -> void {
        event.deregister();
    }

    auto socket::end() const -> void {
        if (::shutdown(descriptor, SHUT_WR) == -1) {
            throw ext::system_error("failed to shutdown further transmissions");
        }

        TIMBER_DEBUG("{} shutdown transmissions", *this);
    }

    auto socket::notify() -> void {
        event.notify();
    }

    auto socket::read(
        void* buffer,
        std::size_t len
    ) -> ext::task<std::size_t> {
        auto bytes = -1;

        do {
            bytes = ::recv(descriptor, buffer, len, 0);

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await wait(EPOLLIN);
                    continue;
                }

                throw ext::system_error("failed to receive data");
            }
        } while (bytes == -1);

        TIMBER_DEBUG("{} recv {} bytes", *this, bytes);

        co_return bytes;
    }

    auto socket::register_scoped() -> register_guard {
        return event.register_scoped();
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

                throw ext::system_error("sendfile failure");
            }

            sent += bytes;

            TIMBER_DEBUG(
                "{} send {} bytes (sendfile [{}/{}])", *this, bytes, sent, count
            );
        }
    }

    auto socket::valid() const -> bool { return descriptor.valid(); }

    auto socket::wait(uint32_t events) -> ext::task<> {
        co_await event.wait(events, nullptr);
    }

    auto socket::write(
        const void* data,
        std::size_t len
    ) -> ext::task<std::size_t> {
        auto bytes = -1;

        do {
            bytes = ::send(descriptor, data, len, MSG_NOSIGNAL);

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await wait(EPOLLOUT);
                    continue;
                }

                TIMBER_DEBUG("{} failed to send data", *this);
                throw ext::system_error("failed to send data");
            }
        } while (bytes == -1);

        TIMBER_DEBUG("{} send {} bytes", *this, bytes);

        co_return bytes;
    }
}
