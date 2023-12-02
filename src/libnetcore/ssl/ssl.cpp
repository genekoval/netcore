#include <netcore/ssl/error.hpp>
#include <netcore/ssl/ssl.hpp>

#include <utility>

namespace netcore::ssl {
    ssl::ssl(SSL* s) : handle(s) {}

    ssl::ssl(const ssl& other) : handle(other.handle) {
        if (handle && !SSL_up_ref(handle)) {
            handle = nullptr;
            throw error("Failed to increment SSL reference count");
        }
    }

    ssl::ssl(ssl&& other) : handle(std::exchange(other.handle, nullptr)) {}

    ssl::~ssl() { SSL_free(handle); }

    auto ssl::operator=(const ssl& other) -> ssl& {
        if (handle != other.handle) {
            SSL_free(std::exchange(handle, nullptr));

            if (other.handle && !SSL_up_ref(other.handle)) {
                throw error("Failed to increment SSL reference count");
            }

            handle = other.handle;
        }

        return *this;
    }

    auto ssl::operator=(ssl&& other) -> ssl& {
        if (handle != other.handle) {
            SSL_free(handle);
            handle = std::exchange(other.handle, nullptr);
        }

        return *this;
    }

    auto ssl::accept() -> int { return SSL_accept(handle); }

    auto ssl::get_error(int ret) const noexcept -> int {
        return SSL_get_error(handle, ret);
    }

    auto ssl::get() const noexcept -> SSL* { return handle; }

    auto ssl::set_fd(int fd) -> void {
        if (SSL_set_fd(handle, fd) != 1) {
            throw error("Failed to connect SSL object with file descriptor");
        }
    }

    auto ssl::shutdown() const noexcept -> int { return SSL_shutdown(handle); }
}
