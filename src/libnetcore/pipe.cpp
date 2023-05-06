#include <netcore/pipe.hpp>

#include <array>
#include <ext/except.h>
#include <fcntl.h>
#include <timber/timber>
#include <unistd.h>
#include <utility>

namespace netcore {
        pipe::pipe() {
            auto descriptors = std::array<int, 2>();

            if (::pipe2(descriptors.data(), O_CLOEXEC | O_NONBLOCK)) {
                throw ext::system_error("Failed to create pipe");
            }

            read_end = descriptors[0];
            write_end = descriptors[1];

            TIMBER_TRACE(
                "pipe (read: {}, write: {}) created",
                static_cast<int>(read_end),
                static_cast<int>(write_end)
            );
        }

        auto pipe::read() -> fd {
            auto r = std::move(read_end);
            const auto w = std::move(write_end);

            return r;
        }

        auto pipe::write() -> fd {
            const auto r = std::move(read_end);
            auto w = std::move(write_end);

            return w;
        }
}
