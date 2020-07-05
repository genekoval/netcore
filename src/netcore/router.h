#pragma once

#include <netcore/socket.h>

namespace netcore::cli {
    auto route(socket&& client) -> void;
}
