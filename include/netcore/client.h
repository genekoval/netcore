#pragma once

#include <netcore/socket.h>

namespace netcore {
    auto connect(std::string_view path) -> socket;
}
