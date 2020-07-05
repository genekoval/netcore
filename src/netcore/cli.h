#pragma once

#include <commline/commline>
#include <timber/timber>

namespace commline {
    template<>
    auto parse(std::string_view argument) -> timber::level;
}
