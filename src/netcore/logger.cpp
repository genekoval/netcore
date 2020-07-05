#include <iostream>
#include <timber/timber>

auto timber::handle_log(const timber::log& log) noexcept -> void {
    std::cerr
        << '[' << log.log_level << "] "
        << log.stream.str()
        << std::endl;
}
