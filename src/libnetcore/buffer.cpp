#include <netcore/buffer.hpp>

#include <cassert>
#include <cstring>

namespace netcore {
    buffer::buffer(std::size_t capacity) :
        storage(new std::byte[capacity]),
        cap(capacity)
    {}

    auto buffer::append(std::size_t bytes) -> void {
        tail += bytes;
    }

    auto buffer::available() const noexcept -> std::size_t {
        return cap - tail;
    }

    auto buffer::back() const noexcept -> const std::byte* {
        return &storage[tail];
    }

    auto buffer::back() noexcept -> std::byte* {
        return &storage[tail];
    }

    auto buffer::capacity() const noexcept -> std::size_t {
        return cap;
    }

    auto buffer::clear() noexcept -> void {
        head = 0;
        tail = 0;
    }

    auto buffer::consume(std::size_t bytes) -> void {
        head += bytes;
        if (head == tail) clear();
    }

    auto buffer::data() const noexcept -> std::span<const std::byte> {
        return {front(), size()};
    }

    auto buffer::empty() const noexcept -> bool {
        return size() == 0;
    }

    auto buffer::front() const noexcept -> const std::byte* {
        return &storage[head];
    }

    auto buffer::front() noexcept -> std::byte* {
        return &storage[head];
    }

    auto buffer::full() const noexcept -> bool {
        return available() == 0;
    }

    auto buffer::read() -> std::span<const std::byte> {
        auto result = data();
        consume(size());
        return result;
    }

    auto buffer::read(std::size_t len) -> std::span<const std::byte> {
        len = std::min(size(), len);
        auto result = std::span<const std::byte>(front(), len);
        consume(len);
        return result;
    }

    auto buffer::read(void* dest, std::size_t len) -> std::size_t {
        return read_bytes(reinterpret_cast<std::byte*>(dest), len);
    }

    auto buffer::read_bytes(std::byte* dest, std::size_t len) -> std::size_t {
        len = std::min(len, size());

        std::memcpy(dest, front(), len);
        consume(len);

        return len;
    }

    auto buffer::size() const noexcept -> std::size_t {
        return tail - head;
    }

    auto buffer::write(const void* src, std::size_t len) -> std::size_t {
        return write_bytes(reinterpret_cast<const std::byte*>(src), len);
    }

    auto buffer::write_bytes(
        const std::byte* src,
        std::size_t len
    ) -> std::size_t {
        len = std::min(len, available());

        std::memcpy(back(), src, len);
        append(len);

        return len;
    }
}
