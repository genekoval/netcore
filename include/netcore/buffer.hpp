#pragma once

#include <memory>
#include <span>

namespace netcore {
    class buffer final {
        std::unique_ptr<std::byte[]> storage;
        std::size_t cap = 0;
        std::size_t head = 0;
        std::size_t tail = 0;

        auto read_bytes(std::byte* dest, std::size_t len) -> std::size_t;

        auto write_bytes(const std::byte* src, std::size_t len) -> std::size_t;
    public:
        buffer() = default;

        explicit buffer(std::size_t capacity);

        auto append(std::size_t bytes) -> void;

        auto available() const noexcept -> std::size_t;

        auto back() const noexcept -> const std::byte*;

        auto back() noexcept -> std::byte*;

        auto capacity() const noexcept -> std::size_t;

        auto clear() noexcept -> void;

        auto consume(std::size_t bytes) -> void;

        auto data() const noexcept -> std::span<const std::byte>;

        auto empty() const noexcept -> bool;

        auto front() const noexcept -> const std::byte*;

        auto front() noexcept -> std::byte*;

        auto full() const noexcept -> bool;

        auto read() -> std::span<const std::byte>;

        auto read(std::size_t len) -> std::span<const std::byte>;

        auto read(void* dest, std::size_t len) -> std::size_t;

        auto size() const noexcept -> std::size_t;

        auto write(const void* src, std::size_t len) -> std::size_t;
    };
}
