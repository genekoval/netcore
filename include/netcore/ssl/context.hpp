#pragma once

#include "socket.hpp"
#include "ssl.hpp"

#include <netcore/socket.h>

#include <filesystem>

namespace netcore::ssl {
    class context {
        SSL_CTX* ctx = nullptr;

        context(const SSL_METHOD* method);
    public:
        static auto server() -> context;

        context() = default;

        context(const context& other);

        context(context&& other);

        ~context();

        auto operator=(const context& other) -> context&;

        auto operator=(context&& other) -> context&;

        auto certificate(const std::filesystem::path& file) -> void;

        auto new_ssl() -> ssl;

        auto private_key(const std::filesystem::path& file) -> void;

        auto set_alpn_select_callback(
            SSL_CTX_alpn_select_cb_func callback,
            void* arg = nullptr
        ) -> void;

        auto set_next_protos_advertised_callback(
            SSL_CTX_npn_advertised_cb_func callback,
            void* arg = nullptr
        ) -> void;

        auto set_options(long options) -> void;

        auto wrap(netcore::socket&& socket) -> netcore::ssl::socket;
    };
}
