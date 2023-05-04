#include <netcore/fd.hpp>

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
        std::destroy_at(this);
        std::construct_at(this, std::forward<fd>(other));
        return *this;
    }

    auto fd::close() noexcept -> void {
        if (::close(descriptor) == -1) {
            TIMBER_ERROR(
                "Failed to close file descriptor ({}): {}",
                descriptor,
                std::strerror(errno)
            );
        }
        else {
            TIMBER_TRACE("fd ({}) closed", descriptor);
        }

        descriptor = invalid;
    }

    auto fd::valid() const -> bool { return descriptor != invalid; }
}
