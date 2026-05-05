#include <fmt/format.h>
#include "predator.h"
#include "prey.h"
#include <algorithm>
#include <cmath>

auto Predator::get_stage() const -> int
{
    if (eaten < 50)  return 0;  // Stalking
    if (eaten < 150) return 1;  // Hunting
    return 2;                   // Apex
}

auto Predator::get_current_speed() const -> double
{
    switch (get_stage())
    {
        case 0: return speed * 0.10;   // Stalking  — slow, cautious
        case 1: return speed * 0.40;   // Hunting   — picking up pace
        default: return speed;         // Apex      — full aggression
    }
}

auto Predator::update(const double time_delta, const Environment& environment, const std::vector<Prey>& prey) -> void
{
    auto dist_to_me = [&](const Prey& p) {
        return std::hypot(p.position[0] - position[0], p.position[1] - position[1]);
    };

    auto closest = std::min_element(prey.begin(), prey.end(), [&](const Prey& a, const Prey& b) {
        return dist_to_me(a) < dist_to_me(b);
    });

    if (closest != prey.end())
    {
        double dx = closest->position[0] - position[0];
        double dy = closest->position[1] - position[1];
        double mag = std::hypot(dx, dy);
        if (mag > 0) { velocity[0] = dx / mag; velocity[1] = dy / mag; }
    }

    double spd = get_current_speed();
    position[0] += velocity[0] * time_delta * spd;
    position[1] += velocity[1] * time_delta * spd;

    environment.restrict_position(position);
    environment.reflect(position, velocity);
    environment.resolve_obstacle_collision(position, velocity);
}

auto Predator::log(const double time) -> void
{
    fmt::print("{:.2f},{:.2f},{:.2f}\n", time, position[0], position[1]);
}