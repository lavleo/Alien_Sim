#pragma once
#include <array>
#include <vector>
#include "environment.h"

struct Predator;

struct Prey
{
    auto update(const double time_delta, const Environment& environment,
                const Predator& predator, const double time,
                const std::vector<const Prey*>& neighbors) -> void;
    auto log(const double time) -> void;

    std::array<double, 2> position{};
    std::array<double, 2> velocity{};
    double speed      = 1.0;
    double time_alive = 0.0;
    int    state      = 0;   // 0=Normal  1=Fleeing  2=Escaping
};