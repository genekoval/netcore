#pragma once

#include <netcore/socket.h>

#include <cstdint>
#include <span>

namespace netcore {
    class signalfd {
        const fd descriptor;
        system_event event;

        explicit signalfd(int descriptor);

        auto read(void* buffer, std::size_t len) -> ext::task<void>;
    public:
        static auto create(std::span<const int> signals) -> signalfd;

        operator int() const;

        auto wait_for_signal() -> ext::task<int>;
    };
}
