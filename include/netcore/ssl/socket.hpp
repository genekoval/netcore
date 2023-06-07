#pragma once

#include "ssl.hpp"

#include <netcore/fd.hpp>
#include <netcore/system_event.hpp>

#include <fmt/format.h>

namespace netcore::ssl {
    class socket {
        fd descriptor;
        system_event event;
        ssl ssl;
    public:
        socket() = default;

        socket(fd&& descriptor, netcore::ssl::ssl&& ssl);

        operator int() const;

        auto accept() -> ext::task<std::string_view>;

        auto read(void* dest, std::size_t len) -> ext::task<std::size_t>;

        auto shutdown() -> std::optional<int>;

        auto try_read(void* dest, std::size_t len) -> long;

        auto write(
            const void* src,
            std::size_t length
        ) -> ext::task<std::size_t>;
    };
}

template <>
struct fmt::formatter<netcore::ssl::socket> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::ssl::socket& socket, FormatContext& ctx) {
        return format_to(
            ctx.out(),
            "SSL socket ({})",
            static_cast<int>(socket)
        );
    }
};
