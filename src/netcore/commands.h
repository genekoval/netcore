#pragma once

#include <commline/commline>

namespace netcore::cli {
    auto start() -> std::unique_ptr<commline::command_node>;
}
