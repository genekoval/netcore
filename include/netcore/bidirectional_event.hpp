#pragma once

#include "system_event.hpp"

namespace netcore {
    class bidirectional_event {
        system_event inner;
        ext::continuation<std::uint32_t> continuation;

        auto wait() -> ext::task<std::uint32_t>;

        auto wait_for(std::uint32_t event) -> ext::task<>;
    public:
        bidirectional_event() = default;

        bidirectional_event(int fd);

        auto cancel() -> void;

        auto in() -> ext::task<>;

        auto out() -> ext::task<>;
    };
}
