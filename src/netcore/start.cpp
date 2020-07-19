#include "cli.h"
#include "commands.h"
#include "router.h"

#include <netcore/server.h>
#include <timber/timber>
#include <unistd.h>

namespace fs = std::filesystem;

static auto $start(
    const commline::app& app,
    const commline::argv& argv,
    timber::level log_level
) -> void {
    timber::reporting_level() = log_level;

    INFO()
        << app.name
        << " version " << app.version
        << " starting: [PID " << getpid() << ']';

    auto server = netcore::server(netcore::cli::route);

    auto sock = fs::temp_directory_path();
    sock /= app.name;
    sock += ".sock";

    server.listen(sock);

    INFO() << "Shuting down...";
}

namespace netcore::cli {
    using namespace commline;

    auto start() -> std::unique_ptr<command_node> {
        return command(
            "start",
            "Start the server.",
            options(
                option<timber::level>(
                    {"log-level", "l"},
                    "Minimum log level to display.",
                    "level",
                    timber::level::info
                )
            ),
            $start
        );
    }
}
