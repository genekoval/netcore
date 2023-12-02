#include <netcore/ssl/context.hpp>
#include <netcore/ssl/error.hpp>

#include <fmt/format.h>
#include <utility>

namespace fs = std::filesystem;

namespace netcore::ssl {
    context::context(const SSL_METHOD* method) : ctx(SSL_CTX_new(method)) {
        if (!ctx) throw error("Failed to create SSL/TLS context");
    }

    context::context(const context& other) : ctx(other.ctx) {
        if (ctx && !SSL_CTX_up_ref(ctx)) {
            ctx = nullptr;
            throw error("Failed to increment SSL context reference count");
        }
    }

    context::context(context&& other) :
        ctx(std::exchange(other.ctx, nullptr)) {}

    context::~context() { SSL_CTX_free(ctx); }

    auto context::operator=(const context& other) -> context& {
        if (ctx != other.ctx) {
            SSL_CTX_free(std::exchange(ctx, nullptr));

            if (other.ctx && !SSL_CTX_up_ref(other.ctx)) {
                throw error("Failed to increment SSL context reference count");
            }

            ctx = other.ctx;
        }

        return *this;
    }

    auto context::operator=(context&& other) -> context& {
        if (ctx != other.ctx) {
            SSL_CTX_free(ctx);
            ctx = std::exchange(other.ctx, nullptr);
        }

        return *this;
    }

    auto context::certificate(const std::filesystem::path& file) -> void {
        if (SSL_CTX_use_certificate_chain_file(ctx, file.c_str()) != 1) {
            throw std::runtime_error(fmt::format(
                "Failed to read certificate chain file '{}'",
                file.native()
            ));
        }
    }

    auto context::new_ssl() -> ssl {
        auto* const s = SSL_new(ctx);
        if (!s) throw error("Failed to create SSL/TLS session object");
        return s;
    }

    auto context::private_key(const fs::path& file) -> void {
        if (SSL_CTX_use_PrivateKey_file(ctx, file.c_str(), SSL_FILETYPE_PEM) !=
            1)
            throw std::runtime_error(fmt::format(
                "Failed to read private key file '{}'",
                file.native()
            ));
    }

    auto context::server() -> context { return context(TLS_server_method()); }

    auto context::set_alpn_select_callback(
        SSL_CTX_alpn_select_cb_func callback,
        void* arg
    ) -> void {
        SSL_CTX_set_alpn_select_cb(ctx, callback, arg);
    }

    auto context::set_next_protos_advertised_callback(
        SSL_CTX_npn_advertised_cb_func callback,
        void* arg
    ) -> void {
        SSL_CTX_set_next_protos_advertised_cb(ctx, callback, arg);
    }

    auto context::set_options(long options) -> void {
        SSL_CTX_set_options(ctx, options);
    }

    auto context::wrap(netcore::socket&& socket) -> netcore::ssl::socket {
        auto [fd, event] = socket.release();

        return netcore::ssl::socket(std::move(fd), std::move(event), new_ssl());
    }
}
