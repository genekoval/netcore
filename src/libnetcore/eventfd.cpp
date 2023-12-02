#include <netcore/eventfd.hpp>

#include <ext/except.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <timber/timber>

namespace netcore {
    eventfd::eventfd() :
        descriptor(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
        event(runtime::event::create(descriptor, EPOLLIN)) {
        if (!descriptor.valid()) {
            throw ext::system_error("Failed to create eventfd");
        }

        TIMBER_TRACE("eventfd ({}) created", descriptor);
    }

    auto eventfd::handle() const noexcept -> eventfd_handle {
        return {descriptor};
    }

    auto eventfd::wait() -> ext::task<std::uint64_t> {
        std::uint64_t value = 0;
        auto retval = -1;

        do {
            retval = eventfd_read(descriptor, &value);

            if (retval == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (co_await event->in()) continue;
                    else co_return 0;
                }

                throw ext::system_error("Failed to read eventfd value");
            }
        } while (retval != 0);

        TIMBER_TRACE("eventfd ({}) read {:L}", descriptor, value);
        co_return value;
    }

    eventfd_handle::eventfd_handle(int descriptor) : descriptor(descriptor) {}

    eventfd_handle::operator bool() const noexcept { return valid(); }

    auto eventfd_handle::set(std::uint64_t value) const -> void {
        if (eventfd_write(descriptor, value) == -1) {
            throw ext::system_error("Failed to write eventfd value");
        }

        TIMBER_TRACE("eventfd ({}) set {:L}", descriptor, value);
    }

    auto eventfd_handle::valid() const noexcept -> bool {
        return descriptor != -1;
    }
}
