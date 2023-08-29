#pragma once

#include "fd.hpp"
#include "system_event.hpp"

#include <cstdint>
#include <span>

namespace netcore {
    class signalfd {
        const fd descriptor;
        system_event event;

        explicit signalfd(int descriptor);
    public:
        static auto create(std::span<const int> signals) -> signalfd;

        operator int() const;

        auto wait_for_signal() -> ext::task<int>;
    };
}
