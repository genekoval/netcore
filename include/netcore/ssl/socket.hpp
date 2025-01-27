#pragma once

#include "ssl.hpp"

#include <netcore/fd.hpp>
#include <netcore/runtime.hpp>

#include <fmt/format.h>

namespace netcore::ssl {
    class socket {
        netcore::fd descriptor;
        std::shared_ptr<runtime::event> event;
        netcore::ssl::ssl ssl;
    public:
        socket() = default;

        socket(
            netcore::fd&& descriptor,
            std::shared_ptr<runtime::event>&& event,
            netcore::ssl::ssl&& ssl
        );

        auto accept() -> ext::task<std::string_view>;

        auto await_read() -> ext::task<>;

        auto await_write() -> ext::task<>;

        auto fd() const noexcept -> int;

        auto read(void* dest, std::size_t len) -> ext::task<std::size_t>;

        auto shutdown() -> std::optional<int>;

        auto try_read(void* dest, std::size_t len) -> long;

        auto try_write(const void* src, std::size_t len) -> long;

        auto write(const void* src, std::size_t length)
            -> ext::task<std::size_t>;
    };
}

template <>
struct fmt::formatter<netcore::ssl::socket> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::ssl::socket& socket, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "SSL socket ({})", socket.fd());
    }
};
