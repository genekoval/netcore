#pragma once

#include "fd.hpp"
#include "runtime.hpp"

#include <ext/coroutine>
#include <fmt/format.h>
#include <sstream>
#include <string>
#include <sys/socket.h>

namespace netcore {
    class socket {
        netcore::fd descriptor;
        bool error = false;
        std::shared_ptr<runtime::event> event;

        [[noreturn]]
        auto failure(const char* message) -> void;
    public:
        socket() = default;

        explicit socket(int fd);

        socket(int domain, int type, int protocol);

        auto await_read() -> ext::task<>;

        auto await_write() -> ext::task<>;

        auto cancel() noexcept -> void;

        auto connect(const sockaddr* addr, socklen_t len) -> ext::task<bool>;

        auto end() const -> void;

        auto failed() const noexcept -> bool;

        auto fd() const noexcept -> int;

        auto read(
            void* dest,
            std::size_t len
        ) -> ext::task<std::size_t>;

        auto release() ->
            std::pair<netcore::fd, std::shared_ptr<runtime::event>>;

        auto sendfile(
            const netcore::fd& descriptor,
            std::size_t count
        ) -> ext::task<>;

        auto try_read(void* dest, std::size_t len) -> long;

        auto try_write(const void* src, std::size_t len) -> long;

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
        return format_to(ctx.out(), "socket ({})", socket.fd());
    }
};
