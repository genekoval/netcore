#include <netcore/eventfd.h>

#include <ext/except.h>
#include <sys/eventfd.h>
#include <timber/timber>

namespace netcore {
    eventfd::eventfd() :
        descriptor(::eventfd(0, EFD_NONBLOCK)),
        notification(descriptor, EPOLLIN)
    {
        if (!descriptor.valid()) {
            throw ext::system_error("failed to create eventfd");
        }

        TIMBER_TRACE("eventfd ({}) created", descriptor);
    }

    auto eventfd::handle() const noexcept -> eventfd_handle {
        return {descriptor};
    }

    auto eventfd::wait() -> ext::task<uint64_t> {
        auto retval = -1;

        do {
            retval = eventfd_read(descriptor, &value);

            if (retval == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    co_await notification.wait();
                    continue;
                }

                throw ext::system_error("failed to read eventfd value");
            }
        } while (retval != 0);

        TIMBER_TRACE("eventfd ({}) read {}", descriptor, value);
        co_return value;
    }

    eventfd_handle::eventfd_handle(int descriptor) : descriptor(descriptor) {}

    eventfd_handle::operator bool() const noexcept {
        return valid();
    }

    auto eventfd_handle::set(uint64_t value) -> void {
        if (eventfd_write(descriptor, value) == -1) {
            throw ext::system_error("failed to write eventfd value");
        }

        TIMBER_TRACE("eventfd ({}) set {}", descriptor, value);
    }

    auto eventfd_handle::valid() const noexcept -> bool {
        return descriptor != -1;
    }
}
