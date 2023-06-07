#pragma once

#include "socket.hpp"

#include <netcore/buffered_reader.hpp>
#include <netcore/buffered_writer.hpp>

namespace netcore::ssl {
    class buffered_socket {
        socket inner;

        buffered_reader<socket> reader;
        buffered_writer<socket> writer;
    public:
        buffered_socket();

        buffered_socket(socket&& socket, std::size_t buffer_size);

        buffered_socket(buffered_socket&& other);

        auto operator=(buffered_socket&& other) -> buffered_socket&;

        operator int() const;

        auto flush() -> ext::task<>;

        auto read() -> ext::task<std::span<const std::byte>>;

        auto shutdown() -> std::optional<int>;

        auto write(const void* src, std::size_t len) -> ext::task<>;
    };
}
