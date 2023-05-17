#include <netcore/except.hpp>

namespace netcore {
    auto eof::what() const noexcept -> const char* {
        return "Unexpected EOF";
    }

    task_canceled::task_canceled() : std::runtime_error("task canceled") {}
}
