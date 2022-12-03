#pragma once

#include "fd.h"
#include "system_event.hpp"

#include <ext/coroutine>
#include <fmt/format.h>
#include <sstream>
#include <string>

namespace netcore {
    class socket {
        fd descriptor;
        system_event event;
    public:
        socket() = default;
        socket(int fd, uint32_t events);
        socket(int domain, int type, uint32_t events);

        operator int() const;

        auto deregister() -> void;

        auto end() const -> void;

        auto notify() -> void;

        auto read(
            void* buffer,
            std::size_t len
        ) -> ext::task<std::size_t>;

        auto register_scoped() -> register_guard;

        auto sendfile(
            const fd& descriptor,
            std::size_t count
        ) -> ext::task<>;

        auto valid() const -> bool;

        auto wait(uint32_t events = 0) -> ext::task<>;

        auto write(
            const void* data,
            std::size_t len
        ) -> ext::task<std::size_t>;
    };
}

template <>
struct fmt::formatter<netcore::socket> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::socket& socket, FormatContext& ctx) {
        return format_to(ctx.out(), "socket ({})", static_cast<int>(socket));
    }
};
