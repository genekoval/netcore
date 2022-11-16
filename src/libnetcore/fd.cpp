#include <netcore/fd.h>

#include <cstring>
#include <ext/except.h>
#include <fcntl.h>
#include <timber/timber>
#include <unistd.h>
#include <utility>

namespace {
    constexpr auto invalid = -1;
}

namespace netcore {
    fd::fd() : descriptor(invalid) {}

    fd::fd(int descriptor) : descriptor(descriptor) {}

    fd::fd(fd&& other) noexcept :
        descriptor(std::exchange(other.descriptor, invalid))
    {}

    fd::~fd() {
        if (descriptor == invalid) return;
        close();
    }

    fd::operator int() const { return descriptor; }

    auto fd::operator=(fd&& other) noexcept -> fd& {
        descriptor = std::exchange(other.descriptor, invalid);
        return *this;
    }

    auto fd::close() noexcept -> void {
        if (::close(descriptor) == -1) {
            TIMBER_ERROR(
                "failed to close file descriptor: {}",
                std::strerror(errno)
            );
        }
        else {
            TIMBER_TRACE("fd ({}) closed", descriptor);
        }

        descriptor = invalid;
    }

    auto fd::flags() const -> int {
        auto fl = fcntl(descriptor, F_GETFL, 0);
        if (fl == -1) {
            throw ext::system_error("failed to get socket flags");
        }

        return fl;
    }

    auto fd::flags(int flags) const -> void {
        auto fl = this->flags() | flags;

        if (fcntl(descriptor, F_SETFL, fl) == -1) {
            throw ext::system_error("failed to set socket flags");
        }
    }

    auto fd::valid() const -> bool { return descriptor != invalid; }
}
