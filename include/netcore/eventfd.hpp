#pragma once

#include "fd.hpp"
#include "runtime.hpp"

namespace netcore {
    class eventfd_handle;

    class eventfd {
        fd descriptor;
        std::shared_ptr<runtime::event> event;
    public:
        eventfd();

        auto handle() const noexcept -> eventfd_handle;

        auto wait() -> ext::task<std::uint64_t>;
    };

    class eventfd_handle {
        friend class eventfd;

        int descriptor = -1;

        eventfd_handle(int descriptor);
    public:
        eventfd_handle() = default;

        explicit operator bool() const noexcept;

        auto set(std::uint64_t value = 1) const -> void;

        auto valid() const noexcept -> bool;
    };
}
