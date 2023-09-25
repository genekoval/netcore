#pragma once

#include "buffered_reader.hpp"
#include "buffered_writer.hpp"
#include "endpoint.hpp"
#include "socket.h"

namespace netcore {
    class buffered_socket {
        friend struct fmt::formatter<netcore::buffered_socket>;

        socket inner;
        buffered_reader<socket> reader;
        buffered_writer<socket> writer;
    public:
        static auto connect(
            const endpoint& endpoint,
            std::size_t buffer_size
        ) -> ext::task<buffered_socket>;

        buffered_socket();

        buffered_socket(socket&& socket, std::size_t buffer_size);

        buffered_socket(buffered_socket&& other);

        auto operator=(buffered_socket&& other) -> buffered_socket&;

        auto await_write() -> ext::task<>;

        auto cancel() noexcept -> void;

        auto connected() -> bool;

        auto consume(std::size_t len) -> void;

        auto failed() const noexcept -> bool;

        auto fd() const noexcept -> int;

        auto fill_buffer() -> ext::task<bool>;

        auto flush() -> ext::task<>;

        auto peek() -> ext::task<std::span<const std::byte>>;

        auto read() -> ext::task<std::span<const std::byte>>;

        auto read(std::size_t len) -> ext::task<std::span<const std::byte>>;

        auto read(void* dest, std::size_t len) -> ext::task<>;

        auto sendfile(
            const netcore::fd& descriptor,
            std::size_t count
        ) -> ext::task<>;

        auto try_write(const void* src, std::size_t len) -> std::size_t;

        auto write(const void* src, std::size_t len) -> ext::task<>;
    };
}

template <>
struct fmt::formatter<netcore::buffered_socket> : formatter<netcore::socket> {
    template <typename FormatContext>
    auto format(const netcore::buffered_socket& socket, FormatContext& ctx) {
        return formatter<netcore::socket>::format(socket.inner, ctx);
    }
};
