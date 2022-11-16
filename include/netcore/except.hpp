#pragma once

#include <stdexcept>

namespace netcore {
    struct task_canceled : std::runtime_error {
        task_canceled();
    };
}
