#pragma once

#include <netcore/detail/notifier.hpp>

namespace netcore {
    class eventfd_handle;

    class eventfd {
        friend class detail::notifier;

        fd descriptor;
        detail::notification notification;
        uint64_t value = 0;
    public:
        eventfd();

        auto handle() const noexcept -> eventfd_handle;

        auto wait() -> ext::task<uint64_t>;
    };

    class eventfd_handle {
        friend class eventfd;

        int descriptor = -1;

        eventfd_handle(int descriptor);
    public:
        eventfd_handle() = default;

        explicit operator bool() const noexcept;

        auto set(uint64_t value = 1) -> void;

        auto valid() const noexcept -> bool;
    };
}
