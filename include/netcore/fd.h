#pragma once

namespace netcore {
    class fd {
        int descriptor;

        auto close() noexcept -> void;
    public:
        fd();
        fd(int descriptor);

        fd(const fd&) = delete;
        fd(fd&& other) noexcept;

        ~fd();

        operator int() const;

        auto operator=(const fd&) -> fd& = delete;
        auto operator=(fd&& other) noexcept -> fd&;

        auto flags() const -> int;

        auto flags(int flags) const -> void;

        auto valid() const -> bool;
    };
}
