#pragma once

#include <netdb.h>
#include <string_view>

namespace netcore {
    class address {
        addrinfo* result;
    public:
        address(std::string_view host, std::string_view port);

        ~address();

        auto data() -> addrinfo*;
    };
}
