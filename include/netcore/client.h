#pragma once

#include <netcore/socket.h>

namespace netcore {
    auto connect(std::string_view host, std::string_view port) -> socket;

    auto connect(std::string_view path) -> socket;
}
