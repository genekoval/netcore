#include "cli.h"

namespace commline {
    template <>
    auto parse(std::string_view argument) -> timber::level {
        auto level = timber::parse_level(argument);

        if (!level) throw commline::cli_error(
            "unknown log level: " + std::string(argument)
        );

        return *level;
    }
}
