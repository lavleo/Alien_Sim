#pragma once
#include <array>
#include <vector>

struct Prey;   // forward-declared so predator.h stays include-free
class  Ship;

struct Predator
{
    void update(double time_delta, const Ship& ship,
                const std::vector<Prey>& prey, double time);

    void set_velocity(double dx, double dy) {
        velocity[0] = dx;
        velocity[1] = dy;
    }

    void log(double time);

    std::array<double,2> position{};
    std::array<double,2> velocity{0, 0};
    double base_speed    {15.0};
    double lock_on_radius{20.0};
    double hatch_time    {20.0};   // seconds before alien can move
    double eaten         {1.0};
    bool   is_caught     {false};
    bool   locked_on     {false};
    bool   is_hatching   {true};
};
