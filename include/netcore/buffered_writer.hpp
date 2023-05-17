#pragma once

#include "buffer.hpp"

#include <ext/coroutine>

namespace netcore {
    template <typename T>
    concept sink = requires(T t, const void* src, std::size_t len) {
        { t.write(src, len) } -> std::same_as<ext::task<std::size_t>>;
    };

    template <sink Sink>
    class buffered_writer final {
        buffer buffer;
        Sink* sink;

        auto write_bytes(const std::byte* src, std::size_t len) -> ext::task<> {
            if (len >= buffer.capacity()) {
                co_await flush();

                while (len > 0) {
                    const auto bytes_written = co_await sink->write(src, len);

                    src += bytes_written;
                    len -= bytes_written;
                }

                co_return;
            }

            while (len > 0) {
                if (buffer.full()) co_await flush();

                const auto bytes_written = buffer.write(src, len);

                src += bytes_written;
                len -= bytes_written;
            }
        }
    public:
        buffered_writer(Sink& sink) : sink(&sink) {}

        buffered_writer(Sink& sink, std::size_t capacity) :
            buffer(capacity),
            sink(&sink)
        {}

        auto clear() -> void {
            buffer.clear();
        }

        auto flush() -> ext::task<> {
            while (!buffer.empty()) {
                const auto chunk = buffer.data();

                const auto bytes_written = co_await sink->write(
                    chunk.data(),
                    chunk.size()
                );

                buffer.consume(bytes_written);
            }
        }

        auto write(const void* src, std::size_t len) -> ext::task<> {
            return write_bytes(reinterpret_cast<const std::byte*>(src), len);
        }

        auto write_to(Sink& sink) noexcept -> void {
            this->sink = &sink;
        }
    };
}
