#pragma once

#include <netcore/pipe.hpp>
#include <netcore/runtime.hpp>

#include <array>

namespace netcore::proc {
    namespace detail {
        class stdio_base {
        protected:
            int descriptor;

            stdio_base(int descriptor);
        };
    }

    struct inherit : detail::stdio_base {
        inherit(int descriptor);
    };

    struct null : detail::stdio_base {
        null(int descriptor);

        auto child() -> void;
    };

    class piped : public detail::stdio_base {
        netcore::pipe pipe;
        netcore::fd fd;
        std::shared_ptr<runtime::event> event;
    public:
        piped(int descriptor);

        piped(piped&& other) = default;

        ~piped();

        auto operator=(piped&& other) -> piped& = default;

        auto child() -> void;

        auto close() -> void;

        auto parent() -> void;

        auto read(void* dest, std::size_t len) -> ext::task<std::size_t>;

        auto write(
            const void* src,
            std::size_t len
        ) -> ext::task<std::size_t>;
    };

    using stdio_type = std::variant<inherit, null, piped>;

    enum class stdio {
        inherit,
        null,
        piped
    };

    class stdio_stream {
        stdio_type type;
    public:
        stdio_stream(int descriptor, stdio stdio);

        auto child() -> void;

        auto close() -> void;

        template <typename T>
        auto is() const -> bool {
            return std::holds_alternative<T>(type);
        }

        auto parent() -> void;

        auto read(void* dest, std::size_t len) -> ext::task<std::size_t>;

        template <typename Container>
        auto read() -> ext::task<Container> {
            constexpr auto scale_factor = 8;

            auto container = Container();
            std::size_t bytes_read = 0;
            std::size_t bytes = 0;

            do {
                container.resize(bytes_read == 0 ?
                    scale_factor : bytes_read * scale_factor
                );

                bytes = co_await read(
                    &container[bytes_read],
                    container.size() - bytes_read
                );

                bytes_read += bytes;
            } while (bytes > 0);

            container.resize(bytes_read);
            co_return container;
        }

        auto write(const void* src, std::size_t len) -> ext::task<std::size_t>;
    };

    using stdio_streams = std::array<stdio_stream, 3>;
}
