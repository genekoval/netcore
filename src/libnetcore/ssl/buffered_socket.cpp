#include <netcore/ssl/buffered_socket.hpp>

namespace netcore::ssl {
    buffered_socket::buffered_socket() : reader(inner), writer(inner) {}

    buffered_socket::buffered_socket(socket&& socket, std::size_t buffer_size) :
        inner(std::forward<netcore::ssl::socket>(socket)),
        reader(inner, buffer_size),
        writer(inner, buffer_size)
    {}

    buffered_socket::buffered_socket(buffered_socket&& other) :
        inner(std::move(other.inner)),
        reader(std::move(other.reader)),
        writer(std::move(other.writer))
    {
        reader.read_from(inner);
        writer.write_to(inner);
    }

    auto buffered_socket::operator=(
        buffered_socket&& other
    ) -> buffered_socket& {
        if (std::addressof(other) != this) {
            std::destroy_at(this);
            std::construct_at(this, std::forward<buffered_socket>(other));
        }

        return *this;
    }

    buffered_socket::operator int() const {
        return inner;
    }

    auto buffered_socket::flush() -> ext::task<> {
        return writer.flush();
    }

    auto buffered_socket::read() -> ext::task<std::span<const std::byte>> {
        return reader.read();
    }

    auto buffered_socket::shutdown() -> std::optional<int> {
        return inner.shutdown();
    }

    auto buffered_socket::write(
        const void* src,
        std::size_t len
    ) -> ext::task<> {
        return writer.write(src, len);
    }
}
