#pragma once

#include "buffer.hpp"

#include <ext/coroutine>

namespace netcore {
    template <typename T>
    concept sink = requires(T t, const void* src, std::size_t len) {
        { t.await_write() } -> std::same_as<ext::task<>>;

        { t.try_write(src, len) } -> std::convertible_to<long>;

        { t.write(src, len) } -> std::same_as<ext::task<std::size_t>>;
    };

    template <sink Sink>
    class buffered_writer final {
        netcore::buffer buffer;
        Sink* sink;

        auto try_write_bytes(const std::byte* src, std::size_t len)
            -> std::size_t {
            std::size_t total = 0;

            if (len >= buffer.capacity()) {
                if (!try_flush()) return total;

                while (total < len) {
                    const auto written =
                        sink->try_write(src + total, len - total);

                    if (written == -1) return total;

                    total += written;
                }
            }
            else {
                while (total < len) {
                    if (buffer.full() && !try_flush()) return total;
                    total += buffer.write(src + total, len - total);
                }
            }

            return total;
        }

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
            sink(&sink) {}

        auto await_write() -> ext::task<> { return sink->await_write(); }

        auto clear() -> void { buffer.clear(); }

        auto flush() -> ext::task<> {
            while (!try_flush()) co_await sink->await_write();
        }

        auto try_flush() -> bool {
            while (!buffer.empty()) {
                const auto chunk = buffer.data();
                const auto written =
                    sink->try_write(chunk.data(), chunk.size());

                if (written > 0) buffer.consume(written);
                else if (written == -1) return false;
            }

            return true;
        }

        auto try_write(const void* src, std::size_t len) -> std::size_t {
            return try_write_bytes(
                reinterpret_cast<const std::byte*>(src),
                len
            );
        }

        auto write(const void* src, std::size_t len) -> ext::task<> {
            return write_bytes(reinterpret_cast<const std::byte*>(src), len);
        }

        auto write_to(Sink& sink) noexcept -> void { this->sink = &sink; }
    };
}
