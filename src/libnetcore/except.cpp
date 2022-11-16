#include <netcore/except.hpp>

namespace netcore {
    task_canceled::task_canceled() : std::runtime_error("task canceled") {}
}
