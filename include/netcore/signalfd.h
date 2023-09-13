#pragma once

#include "fd.hpp"
#include "runtime.hpp"

#include <cstdint>
#include <span>

namespace netcore {
    class signalfd {
        const fd descriptor;
        std::shared_ptr<runtime::event> event;

        explicit signalfd(int descriptor);
    public:
        static auto create(std::span<const int> signals) -> signalfd;

        operator int() const;

        auto wait_for_signal() -> ext::task<int>;
    };
}
