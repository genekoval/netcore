#pragma once

#include <openssl/ssl.h>
#include <string_view>

namespace netcore::ssl {
    class ssl {
        SSL* handle = nullptr;
    public:
        ssl() = default;

        ssl(SSL* s);

        ssl(const ssl& other);

        ssl(ssl&& other);

        ~ssl();

        auto operator=(const ssl& other) -> ssl&;

        auto operator=(ssl&& other) -> ssl&;

        auto accept() -> int;

        auto get() const noexcept -> SSL*;

        auto get_error(int ret) const noexcept -> int;

        auto set_fd(int fd) -> void;

        auto shutdown() const noexcept -> int;
    };
}
