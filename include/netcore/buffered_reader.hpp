#pragma once

#include "buffer.hpp"
#include "except.hpp"

#include <ext/coroutine>

namespace netcore {
    template <typename T>
    concept source = requires(T t, void* dest, std::size_t len) {
        { t.read(dest, len) } -> std::same_as<ext::task<std::size_t>>;

        { t.try_read(dest, len) } -> std::convertible_to<long>;
    };

    template <source Source>
    class buffered_reader final {
        netcore::buffer buffer;
        Source* source;

        auto read_bytes(std::byte* dest, std::size_t len) -> ext::task<> {
            if (len >= buffer.capacity()) {
                if (!buffer.empty()) {
                    const auto bytes_read = buffer.read(dest, len);

                    dest += bytes_read;
                    len -= bytes_read;
                }

                while (len > 0) {
                    const auto bytes_read = co_await source->read(dest, len);
                    if (bytes_read == 0) throw eof();

                    dest += bytes_read;
                    len -= bytes_read;
                }

                co_return;
            }

            while (len > 0) {
                if (buffer.empty() && !co_await fill_buffer()) throw eof();

                const auto bytes_read = buffer.read(dest, len);

                dest += bytes_read;
                len -= bytes_read;
            }
        }
    public:
        buffered_reader(Source& source) : source(&source) {}

        buffered_reader(Source& source, std::size_t capacity) :
            buffer(capacity),
            source(&source)
        {}

        auto clear() noexcept -> void {
            buffer.clear();
        }

        auto consume(std::size_t len) -> void {
            buffer.consume(len);
        }

        auto done() -> bool {
            if (!buffer.empty()) return false;

            const auto bytes_read = source->try_read(
                buffer.back(),
                buffer.available()
            );

            if (bytes_read > 0) buffer.append(bytes_read);
            return bytes_read == 0;
        }

        auto fill_buffer() -> ext::task<bool> {
            const auto bytes = co_await source->read(
                buffer.back(),
                buffer.available()
            );

            buffer.append(bytes);

            co_return bytes > 0;
        }

        auto read() -> ext::task<std::span<const std::byte>> {
            if (buffer.empty()) co_await fill_buffer();
            co_return buffer.read();
        }

        auto read(std::size_t len) -> ext::task<std::span<const std::byte>> {
            if (buffer.empty() && !co_await fill_buffer()) throw eof();
            co_return buffer.read(len);
        }

        auto read(void* dest, std::size_t len) -> ext::task<> {
            return read_bytes(reinterpret_cast<std::byte*>(dest), len);
        }

        auto read_from(Source& source) noexcept -> void {
            this->source = &source;
        }
    };
}
