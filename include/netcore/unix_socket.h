#pragma once

#include <ext/unix.h>

#include <string>
#include <filesystem>
#include <optional>
#include <variant>

namespace netcore {
    struct unix_socket {
        std::filesystem::path path;
        std::optional<std::filesystem::perms> mode;
        std::optional<ext::user> owner;
        std::optional<ext::group> group;

        auto apply_permissions() const -> void;
    };
}
