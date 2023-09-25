#include <netcore/buffered_socket.hpp>
#include <netcore/connect.hpp>

namespace netcore {
    buffered_socket::buffered_socket() : reader(inner), writer(inner) {}

    buffered_socket::buffered_socket(socket&& socket, std::size_t buffer_size) :
        inner(std::forward<netcore::socket>(socket)),
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

    auto buffered_socket::await_write() -> ext::task<> {
        return writer.await_write();
    }

    auto buffered_socket::cancel() noexcept -> void {
        inner.cancel();
    }

    auto buffered_socket::connect(
        const endpoint& endpoint,
        std::size_t buffer_size
    ) -> ext::task<buffered_socket> {
        co_return buffered_socket(
            co_await netcore::connect(endpoint),
            buffer_size
        );
    }

    auto buffered_socket::connected() -> bool {
        return !reader.done();
    }

    auto buffered_socket::consume(std::size_t len) -> void {
        reader.consume(len);
    }

    auto buffered_socket::failed() const noexcept -> bool {
        return inner.failed();
    }

    auto buffered_socket::fd() const noexcept -> int {
        return inner.fd();
    }

    auto buffered_socket::fill_buffer() -> ext::task<bool> {
        return reader.fill_buffer();
    }

    auto buffered_socket::flush() -> ext::task<> {
        return writer.flush();
    }

    auto buffered_socket::peek() -> ext::task<std::span<const std::byte>> {
        return reader.peek();
    }

    auto buffered_socket::read() -> ext::task<std::span<const std::byte>> {
        return reader.read();
    }

    auto buffered_socket::read(
        std::size_t len
    ) -> ext::task<std::span<const std::byte>> {
        return reader.read(len);
    }

    auto buffered_socket::read(void* dest, std::size_t len) -> ext::task<> {
        return reader.read(dest, len);
    }

    auto buffered_socket::sendfile(
        const netcore::fd& descriptor,
        std::size_t count
    ) -> ext::task<> {
        co_await flush();
        co_await inner.sendfile(descriptor, count);
    }

    auto buffered_socket::try_write(
        const void* src,
        std::size_t len
    ) -> std::size_t {
        return writer.try_write(src, len);
    }

    auto buffered_socket::write(
        const void* src,
        std::size_t len
    ) -> ext::task<> {
        return writer.write(src, len);
    }
}
