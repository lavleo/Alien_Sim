#pragma once

#include "prey.h"
#include "environment.h"
#include <array>
#include <vector>
struct Prey;
struct Predator
{
    auto update(const double time_delta, const Environment& environment, const std::vector<Prey>& prey) -> void;


    auto log(const double time) -> void;

    std::array<double, 2> position{};
    std::array<double, 2> velocity{ 5,3 };
    double eaten = 1;
    bool is_caught = false;
};