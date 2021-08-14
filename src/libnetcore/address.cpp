#include "address.h"

#include <ext/except.h>
#include <cstring>

namespace {
    constexpr auto error_message = "failed to determine internet address";
}

namespace netcore {
    address::address(std::string_view host, std::string_view port) {
        auto hints = addrinfo();
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        auto code = getaddrinfo(host.data(), port.data(), &hints, &result);

        if (code == 0) return;

        if (code == EAI_SYSTEM) throw ext::system_error(error_message);

        throw std::runtime_error(
            std::string(error_message) + ": " + gai_strerror(code)
        );
    }

    address::~address() {
        freeaddrinfo(result);
    }

    auto address::data() -> addrinfo* {
        return result;
    }
}
