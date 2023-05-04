#pragma once

#include "fd.hpp"

#include <filesystem>

namespace netcore {
    auto open(const std::filesystem::path& path, int flags) -> fd;
}
