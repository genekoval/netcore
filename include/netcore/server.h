#pragma once

#include <netcore/socket.h>

#include <filesystem>
#include <functional>
#include <sys/socket.h>

namespace netcore {
    using connection_handler = std::function<void(socket&&)>;

    class server {
        const connection_handler on_connection;
        socket sock;

        auto accept() const -> int;
        auto listen(
            int backlog,
            const std::function<void()>& callback
        ) const -> void;
    public:
        server(const connection_handler on_connection);
        server(const server&) = delete;
        server(server&& other) noexcept;

        auto operator=(const server&) -> server& = delete;

        auto listen(
            const std::filesystem::path& path,
            const std::function<void()>& callback,
            int backlog = SOMAXCONN
        ) -> void;

        auto listen(
            const std::filesystem::path& path,
            std::filesystem::perms perms,
            const std::function<void()>& callback,
            int backlog = SOMAXCONN
        ) -> void;
    };

    class fork_server {
        pid_t pid;
        server m_server;
    public:
        enum class process_type {
            client,
            server
        };

        fork_server(const connection_handler on_connection);

        auto start(
            const std::filesystem::path& path,
            std::filesystem::perms perms,
            const std::function<void()>& callback,
            int backlog = SOMAXCONN
        ) -> process_type;

        auto stop() -> int;

        auto stop(int signal) -> int;
    };
}
