#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>

namespace nova::net {
    class socket {
        int file_descriptor;

        std::string fd_error_text() const;
    public:
        socket(int fd);
        ~socket();
        socket(const socket&) = delete;
        socket(socket&& other) noexcept;

        socket& operator=(const socket&) = delete;
        socket& operator=(socket other) noexcept;

        void close();
        int fd() const;
        std::string receive() const;
        void send(const std::string& data) const;
        bool valid() const;
    };

    class event_monitor {
        socket epoll_sock;
        epoll_event monitor;
        std::unique_ptr<epoll_event[]> events;
        int max_events;
    public:
        event_monitor(int max_events);

        void add(int fd);
        uint32_t get_types();
        void set_types(uint32_t event_types);
        void wait(const std::function<void(int)>& event_handler);
    };

    using sock_ptr = std::shared_ptr<socket>;

    class server {
        static int accept(int fd);

        std::function<void(sock_ptr)> connection_handler;

        void listen(const socket& sock, int backlog) const;
    public:
        server(std::function<void(sock_ptr)> connection_handler);

        void listen(
            const std::filesystem::path& path,
            int backlog = SOMAXCONN
        ) const;
        void listen(
            const std::filesystem::path& path,
            const std::filesystem::perms perms,
            int backlog = SOMAXCONN
        ) const;
    };

    class signalfd {
        socket sock;

        signalfd(int fd);
    public:
        static signalfd create(const std::vector<int>& signals);

        int fd() const;
        uint32_t status() const;
    };
}
