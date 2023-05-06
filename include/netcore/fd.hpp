#pragma once

namespace netcore {
    class fd {
        int descriptor;
    public:
        fd();
        fd(int descriptor);

        fd(const fd&) = delete;
        fd(fd&& other) noexcept;

        ~fd();

        operator int() const;

        auto operator=(const fd&) -> fd& = delete;
        auto operator=(fd&& other) noexcept -> fd&;

        auto close() noexcept -> void;

        auto release() noexcept -> int;

        auto valid() const -> bool;
    };

    auto close(int fd) noexcept -> void;
}
