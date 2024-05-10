#pragma once

#include <array>
#include "environment.h"
#include "predator.h"
struct Predator;

struct Prey
{
    auto update(const double time_delta, const Environment& environment, const Predator& predator, const double time) -> void;

    auto log(const double time) -> void;

    std::array<double, 2> position{};
    std::array<double, 2> velocity{};
};