#include <netcore/runtime.hpp>

#include <filesystem>
#include <fmt/chrono.h>
#include <fmt/os.h>
#include <gtest/gtest.h>
#include <timber/timber>

namespace fs = std::filesystem;

using namespace std::chrono_literals;

namespace {
    const auto log_path = fs::temp_directory_path() / "netcore.test.log";
    auto log_file = fmt::output_file(log_path.native());

    std::mutex mutex;

    auto file_logger(const timber::log& log) noexcept -> void {
        auto lock = std::scoped_lock<std::mutex>(mutex);

        log_file.print("{:%b %m %r}", log.timestamp);
        log_file.print(" {:9} ", log.log_level);
        log_file.print("[{}] ", log.thread_name);
        log_file.print("{}\n", log.message);

        log_file.flush();
    }
}

auto main(int argc, char** argv) -> int {
    timber::thread_name = std::string("main");
    timber::log_handler = &file_logger;

    const auto runtime = netcore::runtime();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
