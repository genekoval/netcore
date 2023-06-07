#include <netcore/ssl/error.hpp>

#include <fmt/format.h>
#include <openssl/err.h>

namespace netcore::ssl {
    error::error() :
        std::runtime_error(ERR_error_string(ERR_get_error(), nullptr))
    {}

    error::error(std::string_view what) :
        std::runtime_error(fmt::format(
            "{}: {}",
            what,
            ERR_error_string(ERR_get_error(), nullptr)
        ))
    {}
}
