#pragma once

#include <netcore/socket.h>

#include <vector>

namespace netcore {
    class signalfd {
        socket sock;

        signalfd(int fd);
    public:
        static auto create(const std::vector<int>& signals) -> signalfd;

        auto fd() const -> int;
        auto signal() const -> uint32_t;
    };
}
