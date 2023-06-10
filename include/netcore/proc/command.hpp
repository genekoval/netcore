#pragma once

#include "child.hpp"

#include <filesystem>
#include <fmt/format.h>
#include <vector>

namespace netcore::proc {
    class arguments {
        friend struct fmt::formatter<arguments>;

        std::vector<char*> storage;
    public:
        arguments() = default;

        arguments(const arguments&) = delete;

        arguments(arguments&&) = default;

        ~arguments();

        auto operator=(const arguments&) -> arguments& = delete;

        auto operator=(arguments&&) -> arguments& = default;

        auto operator[](std::size_t index) -> char*;

        auto append(std::string_view arg) -> void;

        auto append_null() -> void;

        auto get() noexcept -> char**;

        auto size() const noexcept -> std::size_t;
    };

    class command {
        arguments argv;
        std::optional<std::filesystem::path> directory;
        std::array<proc::stdio, 3> stdio {
            proc::stdio::null,
            proc::stdio::inherit,
            proc::stdio::inherit
        };

        [[noreturn]]
        auto exec() noexcept -> void;
    public:
        command(std::string_view program);

        auto arg(std::string_view arg) -> command&;

        template <std::convertible_to<std::string_view> S>
        auto args(std::initializer_list<S> args) -> command& {
            for (auto&& arg : args) argv.append(arg);
            return *this;
        }

        auto err(proc::stdio stdio) -> command&;

        auto in(proc::stdio stdio) -> command&;

        auto out(proc::stdio stdio) -> command&;

        auto spawn() -> child;

        auto working_directory(
            const std::filesystem::path& path
        ) -> command&;
    };

    template <typename T = void, typename... Args>
    auto exec(std::string_view program, Args&&... args) -> ext::task<T> {
        constexpr auto is_void = std::is_same_v<T, void>;

        auto command = proc::command(program);
        (command.arg(args), ...);
        command.err(stdio::piped);

        if constexpr (is_void) command.out(stdio::null);
        else command.out(stdio::piped);

        auto child = command.spawn();
        co_await child.check_return_code();

        if constexpr (!is_void) co_return co_await child.out().read<T>();
    }
}

template <>
struct fmt::formatter<netcore::proc::arguments> : formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const netcore::proc::arguments& arguments, FormatContext& ctx) {
        if (arguments.storage.empty()) {
            return formatter<std::string_view>::format(
                "<no arguments>",
                ctx
            );
        }

        auto buffer = memory_buffer();
        auto it = std::back_inserter(buffer);

        format_to(it, "{}", arguments.storage[0]);

        for (auto i = 1ul; i < arguments.storage.size(); ++i) {
            format_to(it, " `{}`", arguments.storage[i]);
        }

        return formatter<std::string_view>::format(
            {buffer.data(), buffer.size()},
            ctx
        );
    }
};
