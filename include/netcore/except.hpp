#pragma once

#include <stdexcept>

namespace netcore {
    struct eof : std::exception {
        auto what() const noexcept -> const char* override;
    };

    struct task_canceled : std::runtime_error {
        task_canceled();
    };
}
