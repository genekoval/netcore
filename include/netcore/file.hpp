#pragma once

#include <netcore/fd.h>

#include <filesystem>

namespace netcore {
    auto open(const std::filesystem::path& path, int flags) -> fd;
}
