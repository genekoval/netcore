#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <variant>

namespace netcore {
    using owner_t = std::variant<std::string, uid_t>;
    using group_t = std::variant<std::string, gid_t>;

    struct unix_socket {
        std::filesystem::path path;
        std::optional<std::filesystem::perms> mode;
        std::optional<owner_t> owner;
        std::optional<group_t> group;

        auto apply_permissions() const -> void;
    };
}
