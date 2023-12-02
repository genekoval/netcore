#include <netcore/except.hpp>
#include <netcore/ssl/error.hpp>
#include <netcore/ssl/socket.hpp>

#include <ext/except.h>
#include <fmt/format.h>
#include <openssl/err.h>
#include <openssl/sslerr.h>
#include <sys/epoll.h>
#include <timber/timber>

namespace {
    constexpr auto read_error = "SSL socket failed to read data";
}

namespace netcore::ssl {
    socket::socket(
        netcore::fd&& descriptor,
        std::shared_ptr<runtime::event>&& event,
        netcore::ssl::ssl&& ssl
    ) :
        descriptor(std::forward<netcore::fd>(descriptor)),
        event(std::forward<std::shared_ptr<runtime::event>>(event)),
        ssl(std::forward<netcore::ssl::ssl>(ssl)) {
        this->ssl.set_fd(this->descriptor);
    }

    auto socket::accept() -> ext::task<std::string_view> {
        while (true) {
            const auto ret = ssl.accept();
            if (ret == 1) break;

            switch (ssl.get_error(ret)) {
                case SSL_ERROR_WANT_READ: co_await await_read(); break;
                case SSL_ERROR_WANT_WRITE: co_await await_write(); break;
                default:
                    throw error(
                        "Failed to complete TLS/SSL handshake with client"
                    );
            }
        }

        const unsigned char* alpn = nullptr;
        unsigned int len = 0;

        SSL_get0_next_proto_negotiated(ssl.get(), &alpn, &len);
        if (!alpn) SSL_get0_alpn_selected(ssl.get(), &alpn, &len);

        if (!alpn) {
            TIMBER_TRACE("Client did not select a protocol");
            co_return std::string_view();
        }

        auto protocol =
            std::string_view(reinterpret_cast<const char*>(alpn), len);

        TIMBER_TRACE("{} client selected protocol: {}", *this, protocol);

        co_return protocol;
    }

    auto socket::await_read() -> ext::task<> {
        if (!co_await event->in()) throw task_canceled();
    }

    auto socket::await_write() -> ext::task<> {
        if (!co_await event->out()) throw task_canceled();
    }

    auto socket::fd() const noexcept -> int { return descriptor; }

    auto socket::read(void* dest, std::size_t len) -> ext::task<std::size_t> {
        while (true) {
            const auto read = try_read(dest, len);
            if (read >= 0) co_return read;

            co_await await_read();
        }
    }

    auto socket::shutdown() -> std::optional<int> {
        const auto ret = ssl.shutdown();

        switch (ret) {
            case 0: TIMBER_DEBUG("{} sent close_notify", *this); break;
            case 1: TIMBER_DEBUG("{} shutdown complete", *this); break;
            default: return ssl.get_error(ret);
        }

        return std::nullopt;
    }

    auto socket::try_read(void* dest, std::size_t len) -> long {
        errno = 0;

        const auto ret = SSL_read(ssl.get(), dest, len);
        if (ret > 0) {
            TIMBER_TRACE(
                "{} read {:L} byte{}",
                *this,
                ret,
                ret == 1 ? "" : "s"
            );
            return ret;
        }

        switch (ssl.get_error(ret)) {
            case SSL_ERROR_ZERO_RETURN:
                TIMBER_TRACE("{} received EOF", *this);
                return 0;
            case SSL_ERROR_WANT_READ:
                TIMBER_TRACE("{} wants to read", *this);
                return -1;
            case SSL_ERROR_SYSCALL:
                if (ERR_peek_error()) throw error(read_error);
                if (errno) throw ext::system_error(read_error);
                throw std::runtime_error(read_error);
            case SSL_ERROR_SSL:
                if (ERR_GET_REASON(ERR_peek_error()) ==
                    SSL_R_UNEXPECTED_EOF_WHILE_READING) {
                    ERR_get_error(); // Remove the error from the queue.
                    throw eof();
                }
                [[fallthrough]];
            default: throw error(read_error);
        }
    }

    auto socket::try_write(const void* src, std::size_t len) -> long {
        const auto ret = SSL_write(ssl.get(), src, len);
        if (ret > 0) {
            TIMBER_TRACE(
                "{} write {:L} byte{}",
                *this,
                ret,
                ret == 1 ? "" : "s"
            );
            return ret;
        }

        switch (ssl.get_error(ret)) {
            case SSL_ERROR_WANT_WRITE:
                TIMBER_TRACE("{} wants to write", *this);
                return -1;
            default: throw error("SSL socket failed to write data");
        }
    }

    auto socket::write(const void* src, std::size_t len)
        -> ext::task<std::size_t> {
        while (true) {
            const auto written = try_write(src, len);
            if (written >= 0) co_return written;

            co_await await_write();
        }
    }
}
