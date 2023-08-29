#pragma once

#include <ext/unix.h>

#include <filesystem>
#include <optional>
#include <variant>

namespace netcore {
    struct inet_socket {
        std::string host;
        std::string port;
    };

    struct unix_socket {
        std::filesystem::path path;
        std::optional<std::filesystem::perms> mode;
        std::optional<ext::user> owner;
        std::optional<ext::group> group;

        auto apply_permissions() const -> void;

        auto remove() const noexcept -> void;
    };

    using endpoint = std::variant<inet_socket, unix_socket>;

    auto parse_endpoint(std::string_view string) -> endpoint;
}
