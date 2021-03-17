#pragma once

#include <netcore/fd.h>

#include <cstdint>
#include <span>

namespace netcore {
    class signalfd {
        const fd descriptor;

        signalfd(int descriptor);
    public:
        static auto create(std::span<const int> signals) -> signalfd;

        operator int() const;

        auto signal() const -> std::uint32_t;
    };
}
