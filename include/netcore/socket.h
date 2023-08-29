#pragma once

#include "bidirectional_event.hpp"
#include "fd.hpp"

#include <ext/coroutine>
#include <fmt/format.h>
#include <sstream>
#include <string>
#include <sys/socket.h>

namespace netcore {
    class socket {
        fd descriptor;
        bool error = false;
        bidirectional_event event;

        [[noreturn]]
        auto failure(const char* message) -> void;
    public:
        socket() = default;

        explicit socket(int fd);

        socket(int domain, int type, int protocol);

        operator int() const;

        auto cancel() noexcept -> void;

        auto connect(const sockaddr* addr, socklen_t len) -> ext::task<bool>;

        auto end() const -> void;

        auto failed() const noexcept -> bool;

        auto read(
            void* dest,
            std::size_t len
        ) -> ext::task<std::size_t>;

        auto sendfile(
            const fd& descriptor,
            std::size_t count
        ) -> ext::task<>;

        auto take() -> fd;

        auto try_read(void* dest, std::size_t len) -> long;

        auto valid() const -> bool;

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
