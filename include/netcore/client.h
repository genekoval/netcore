#pragma once

#include <netcore/socket.h>

namespace netcore {
    auto connect(
        std::string_view host,
        std::string_view port
    ) -> ext::task<socket>;

    auto connect(std::string_view path) -> ext::task<socket>;
}
