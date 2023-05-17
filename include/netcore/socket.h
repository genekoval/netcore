#pragma once

#include "fd.hpp"
#include "system_event.hpp"

#include <ext/coroutine>
#include <fmt/format.h>
#include <sstream>
#include <string>

namespace netcore {
    class socket {
        fd descriptor;
        bool error = false;
        system_event event;

        [[noreturn]]
        auto failure(const char* message) -> void;
    public:
        socket() = default;
        socket(int fd, uint32_t events);
        socket(int domain, int type, int protocol, uint32_t events);

        operator int() const;

        auto cancel() noexcept -> void;

        auto end() const -> void;

        auto failed() const noexcept -> bool;

        auto notify() -> void;

        auto read(
            void* dest,
            std::size_t len
        ) -> ext::task<std::size_t>;

        auto sendfile(
            const fd& descriptor,
            std::size_t count
        ) -> ext::task<>;

        auto try_read(void* dest, std::size_t len) -> long;

        auto valid() const -> bool;

        auto wait(uint32_t events = 0) -> ext::task<>;

        auto write(
            const void* src,
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
