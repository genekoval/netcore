#pragma once

#include <stdexcept>

namespace netcore::ssl {
    struct error : std::runtime_error {
        error();

        error(std::string_view what);
    };
}
