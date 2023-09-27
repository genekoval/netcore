#pragma once

#include "socket.hpp"

#include <netcore/buffered_reader.hpp>
#include <netcore/buffered_writer.hpp>

namespace netcore::ssl {
    class buffered_socket {
        friend struct fmt::formatter<buffered_socket>;

        socket inner;

        buffered_reader<socket> reader;
        buffered_writer<socket> writer;
    public:
        buffered_socket();

        buffered_socket(socket&& socket, std::size_t buffer_size);

        buffered_socket(buffered_socket&& other);

        auto operator=(buffered_socket&& other) -> buffered_socket&;

        auto fd() const noexcept -> int;

        auto flush() -> ext::task<>;

        auto read() -> ext::task<std::span<const std::byte>>;

        auto shutdown() -> std::optional<int>;

        auto write(const void* src, std::size_t len) -> ext::task<>;
    };
}

template <>
struct fmt::formatter<netcore::ssl::buffered_socket> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(
        const netcore::ssl::buffered_socket& socket,
        FormatContext& ctx
    ) {
        return fmt::format_to(ctx.out(), "{}", socket.inner);
    }
};
