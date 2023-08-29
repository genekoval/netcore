#include <netcore/endpoint.hpp>

#include <timber/timber>

namespace netcore {
    auto unix_socket::apply_permissions() const -> void {
        if (mode) std::filesystem::permissions(path, mode.value());
        ext::chown(path, owner, group);
    }

    auto unix_socket::remove() const noexcept -> void {
        auto error = std::error_code();
        const auto removed = std::filesystem::remove(path, error);

        if (error) {
            TIMBER_ERROR(
                R"(Failed to remove socket file "{}": {})",
                path.native(),
                error.message()
            );
        }
        else if (removed) {
            TIMBER_DEBUG(R"(Removed socket file "{}")", path.native());
        }
    }

    auto parse_endpoint(std::string_view string) -> endpoint {
        if (string.starts_with("/")) {
            return unix_socket { .path = string };
        }

        const auto separator = string.find(":");

        if (separator == std::string_view::npos) {
            return inet_socket { .port = std::string(string) };
        }

        return inet_socket {
            .host = std::string(string.substr(0, separator)),
            .port = std::string(string.substr(separator + 1))
        };
    }
}
