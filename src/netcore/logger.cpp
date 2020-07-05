#include <iostream>
#include <timber/timber>

auto timber::write(const timber::log& log) noexcept -> void {
    std::cerr
        << '[' << log.log_level << "] "
        << log.stream.str()
        << std::endl;
}
