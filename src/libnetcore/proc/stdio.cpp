#include <netcore/proc/stdio.hpp>
#include <netcore/except.hpp>
#include <netcore/file.hpp>

#include <ext/except.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <sys/epoll.h>
#include <timber/timber>
#include <unistd.h>

using netcore::proc::inherit;
using netcore::proc::null;
using netcore::proc::piped;
using netcore::proc::stdio;
using netcore::proc::stdio_type;

namespace {
    auto make_stdio(int descriptor, stdio stdio) -> stdio_type {
        switch (stdio) {
            case stdio::inherit:
                return inherit(descriptor);
            case stdio::null:
                return null(descriptor);
            case stdio::piped:
                return piped(descriptor);
        }

        __builtin_unreachable();
    }
}

namespace netcore::proc {
    namespace detail {
        stdio_base::stdio_base(int descriptor) : descriptor(descriptor) {}
    }

    inherit::inherit(int descriptor) : stdio_base(descriptor) {}

    null::null(int descriptor) : stdio_base(descriptor) {}

    auto null::child() -> void {
        auto file = netcore::open(
            "/dev/null",
            descriptor == STDIN_FILENO ? O_RDONLY : O_WRONLY
        );

        if (dup2(file.release(), descriptor) == -1) {
            fmt::print(
                stderr,
                "Failed to redirect file descriptor ({}) to '/dev/null': {}\n",
                descriptor,
                std::strerror(errno)
            );

            exit(errno);
        }
    }

    piped::piped(int descriptor) : stdio_base(descriptor) {}

    auto piped::child() -> void {
        fd = descriptor == STDIN_FILENO ? pipe.read() : pipe.write();

        if (dup2(fd.release(), descriptor) == -1) {
            fmt::print(
                stderr,
                "Failed to redirect file descriptor ({} -> {}): {}\n",
                static_cast<int>(fd),
                descriptor,
                std::strerror(errno)
            );

            exit(errno);
        }
    }

    auto piped::close() -> void {
        fd.close();
    }

    auto piped::parent() -> void {
        fd = descriptor == STDIN_FILENO ? pipe.write() : pipe.read();
        event = runtime::event::create(
            descriptor,
            descriptor == STDIN_FILENO ? EPOLLIN : EPOLLOUT
        );
    }

    auto piped::read(void* dest, std::size_t len) -> ext::task<std::size_t> {
        auto bytes = -1;

        do {
            bytes = ::read(fd, dest, len);

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (!co_await event->in()) throw task_canceled();
                }
                else {
                    TIMBER_DEBUG("fd ({}) failed to read data", fd);
                    throw ext::system_error("Failed to read data");
                }
            }
        } while (bytes == -1);

        TIMBER_TRACE(
            "fd ({}) read {:L} byte{}",
            fd,
            bytes,
            bytes == 1 ? "" : "s"
        );

        co_return bytes;
    }

    auto piped::write(
        const void* src,
        std::size_t len
    ) -> ext::task<std::size_t> {
        auto bytes = -1;

        do {
            bytes = ::write(fd, src, len);

            if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (!co_await event->out()) throw task_canceled();
                }
                else {
                    TIMBER_DEBUG("fd ({}) failed to write data", fd);
                    throw ext::system_error("Failed to write data");
                }
            }
        } while (bytes == -1);

        TIMBER_TRACE(
            "fd ({}) write {:L} byte{}",
            fd,
            bytes,
            bytes == 1 ? "" : "s"
        );

        co_return bytes;
    }

    stdio_stream::stdio_stream(int descriptor, stdio stdio) :
        type(make_stdio(descriptor, stdio))
    {}

    auto stdio_stream::child() -> void {
        std::visit([](auto&& stream) {
            using T = std::decay_t<decltype(stream)>;

            if constexpr (
                std::is_same_v<T, null> || std::is_same_v<T, piped>
            ) stream.child();
        }, type);
    }

    auto stdio_stream::close() -> void {
        if (auto* const stream = std::get_if<piped>(&type)) stream->close();
    }

    auto stdio_stream::parent() -> void {
        if (auto* const stream = std::get_if<piped>(&type)) stream->parent();
    }

    auto stdio_stream::read(
        void* dest,
        std::size_t len
    ) -> ext::task<std::size_t> {
        if (auto* const stream = std::get_if<piped>(&type)) {
            return stream->read(dest, len);
        }
        else throw std::logic_error("stream unavailable for reading");
    }

    auto stdio_stream::write(
        const void* src,
        std::size_t len
    ) -> ext::task<std::size_t> {
        if (auto* const stream = std::get_if<piped>(&type)) {
            return stream->write(src, len);
        }
        else throw std::logic_error("stream unavailable for writing");
    }
}
