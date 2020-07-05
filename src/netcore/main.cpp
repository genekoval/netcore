#include "commands.h"

#include <iostream>

constexpr auto description = std::string_view("A test application.");

static auto $main(
    const commline::app& app,
    const commline::argv& argv
) -> void {
    std::cout
        << app.name << " " << "v" << app.version << '\n'
        << app.description << std::endl;
}

auto main(int argc, const char** argv) -> int {
    auto app = commline::application(
        NAME,
        VERSION,
        description,
        $main
    );

    app.subcommand(netcore::cli::start());

    return app.run(argc, argv);
}
