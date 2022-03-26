#include <netcore/fd.h>

#include <cstring>
#include <timber/timber>
#include <unistd.h>
#include <utility>

template <>
struct fmt::formatter<netcore::fd> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const netcore::fd& fd, FormatContext& ctx) {
        return format_to(ctx.out(), "fd ({})", static_cast<int>(fd));
    }
};

namespace {
    constexpr auto invalid = -1;
}

namespace netcore {
    fd::fd() : descriptor(invalid) {
        TIMBER_TRACE("file descriptor default created");
    }

    fd::fd(int descriptor) : descriptor(descriptor) {
        TIMBER_DEBUG("{} created", *this);
    }

    fd::fd(fd&& other) noexcept :
        descriptor(std::exchange(other.descriptor, invalid))
    {
        TIMBER_TRACE("{} move constructed", *this);
    }

    fd::~fd() {
        if (descriptor == invalid) return;
        close();
    }

    fd::operator int() const { return descriptor; }

    auto fd::operator=(fd&& other) noexcept -> fd& {
        descriptor = std::exchange(other.descriptor, invalid);
        TIMBER_TRACE("{} move assigned", *this);
        return *this;
    }

    auto fd::close() noexcept -> void {
        if (::close(descriptor) == -1) {
            TIMBER_ERROR("{} failed to close: {}", *this, std::strerror(errno));
        }
        else {
            TIMBER_TRACE("{} closed", *this);
        }

        descriptor = invalid;
    }

    auto fd::valid() const -> bool { return descriptor != invalid; }
}
