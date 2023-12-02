#include <netcore/address.hpp>

#include <cstring>
#include <ext/except.h>

namespace {
    constexpr auto error_message = "failed to determine internet address";
}

namespace netcore {
    address::address(std::string_view host, std::string_view port) {
        auto hints = addrinfo();
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (host.empty()) hints.ai_flags = AI_PASSIVE;

        auto code = getaddrinfo(
            host.empty() ? nullptr : host.data(),
            port.data(),
            &hints,
            &info
        );

        if (code == 0) return;

        if (code == EAI_SYSTEM) throw ext::system_error(error_message);

        throw std::runtime_error(
            std::string(error_message) + ": " + gai_strerror(code)
        );
    }

    address::~address() {
        if (info) freeaddrinfo(info);
    }

    auto address::operator*() const noexcept -> addrinfo& { return *info; }

    auto address::operator->() const noexcept -> addrinfo* { return info; }

    auto address::get() -> addrinfo* { return info; }

    socket_addr::socket_addr(sockaddr* addr, unsigned int addrlen) {
        switch (addr->sa_family) {
            case AF_INET: family_ = ip_addr::ipv4; break;
            case AF_INET6: family_ = ip_addr::ipv6; break;
            default:
                throw std::runtime_error(fmt::format(
                    "unexpected socket address family: {}",
                    addr->sa_family
                ));
        }

        auto host_buffer = std::array<char, NI_MAXHOST>();
        auto port_buffer = std::array<char, NI_MAXSERV>();

        const auto code = getnameinfo(
            addr,
            addrlen,
            host_buffer.data(),
            host_buffer.size(),
            port_buffer.data(),
            port_buffer.size(),
            NI_NUMERICHOST | NI_NUMERICSERV
        );

        if (code == 0) {
            host_ = host_buffer.data();
            port_ = std::stoi(port_buffer.data());
            return;
        }

        if (code == EAI_SYSTEM)
            throw ext::system_error("address-to-name translation failure");

        throw std::runtime_error(gai_strerror(code));
    }

    auto socket_addr::family() const noexcept -> ip_addr { return family_; }

    auto socket_addr::host() const noexcept -> std::string_view {
        return host_;
    }

    auto socket_addr::port() const noexcept -> unsigned short { return port_; }
}
