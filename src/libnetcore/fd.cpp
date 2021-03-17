#include <netcore/fd.h>

#include <cstring>
#include <ostream>
#include <timber/timber>
#include <unistd.h>
#include <utility>

namespace netcore {
    constexpr auto invalid = -1;

    auto operator<<(std::ostream& os, const fd* fd) -> std::ostream& {
        os << "fd (" << static_cast<int>(*fd) << ")";
        return os;
    }

    fd::fd() : descriptor(invalid) {
        TRACE() << "file descriptor default created";
    }

    fd::fd(int descriptor) : descriptor(descriptor) {
        DEBUG() << this << " created";
    }

    fd::fd(fd&& other) noexcept :
        descriptor(std::exchange(other.descriptor, invalid))
    {
        TRACE() << this << " move constructed";
    }

    fd::~fd() {
        if (descriptor == invalid) return;
        close();
    }

    fd::operator int() const { return descriptor; }

    auto fd::operator=(fd&& other) noexcept -> fd& {
        descriptor = std::exchange(other.descriptor, invalid);
        TRACE() << this << " move assigned";
        return *this;
    }

    auto fd::close() noexcept -> void {
        if (::close(descriptor) == -1) {
            ERROR() << this << " failed to close: " << std::strerror(errno);
        }
        else {
            DEBUG() << this << " closed";
        }

        descriptor = invalid;
    }

    auto fd::valid() const -> bool { return descriptor != invalid; }
}
