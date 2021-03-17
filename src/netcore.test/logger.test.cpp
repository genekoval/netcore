#include <filesystem>
#include <fstream>
#include <timber/timber>

constexpr auto log_name = "netcore.test.log";
const auto log_dir = std::filesystem::path(TESTDIR);
const auto log_path = log_dir / log_name;
auto log_file = std::ofstream(log_path);

namespace timber {
    auto handle_log(const log& l) noexcept -> void {
        log_file
            << "[" << l.log_level << "] "
            << l.stream.str()
            << std::endl;
    }
}
