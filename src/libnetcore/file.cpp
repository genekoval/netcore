#include <netcore/file.hpp>

#include <ext/except.h>
#include <fcntl.h>
#include <timber/timber>

namespace netcore {
    auto open(const std::filesystem::path& path, int flags) -> fd {
        const auto result = ::open(path.c_str(), flags);

        if (result == -1) {
            throw ext::system_error(fmt::format(
                R"(could not open file: "{}")",
                path.native()
            ));
        }

        TIMBER_DEBUG(
            R"(opened file ({}): "{}")",
            result,
            path.native()
        );

        return result;
    }
}
