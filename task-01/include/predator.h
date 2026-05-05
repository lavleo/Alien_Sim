#pragma once
#include "environment.h"
#include <array>
#include <vector>

struct Prey;

struct Predator
{
    auto update(const double time_delta, const Environment& environment, const std::vector<Prey>& prey) -> void;
    auto log(const double time) -> void;
    auto get_stage() const -> int;         // 0=Stalking, 1=Hunting, 2=Apex
    auto get_current_speed() const -> double;

    std::array<double, 2> position{};
    std::array<double, 2> velocity{ 5, 3 };
    double eaten = 0;
    double speed = 15.0;   // set from config
};