#pragma once

#include "runtime.hpp"

#include <cstdint>
#include <ext/coroutine>

namespace netcore {
    class system_event {
        runtime::awaiter awaiter;
    public:
        system_event() = default;

        system_event(int fd);

        auto add(std::uint32_t events) -> void;

        auto awaiting() const noexcept -> bool;

        auto cancel() -> void;

        auto in() -> ext::task<bool>;

        auto out() -> ext::task<bool>;

        auto set(std::uint32_t events) -> void;

        auto wait() -> ext::task<std::uint32_t>;

        auto wait_for(std::uint32_t events) -> ext::task<std::uint32_t>;
    };
}
