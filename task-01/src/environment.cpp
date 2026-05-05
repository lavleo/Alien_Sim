#include <algorithm>
#include <cmath>
#include "environment.h"

auto Environment::restrict_position(std::array<double, 2>& position) const -> void
{
    position[0] = std::clamp(position[0], x_limit[0], x_limit[1]);
    position[1] = std::clamp(position[1], y_limit[0], y_limit[1]);
}

auto Environment::reflect(const std::array<double, 2>& position, std::array<double, 2>& velocity) const -> void
{
    if (position[0] == x_limit[0] || position[0] == x_limit[1])
        velocity[0] *= -1.0;
    if (position[1] == y_limit[0] || position[1] == y_limit[1])
        velocity[1] *= -1.0;
}

auto Environment::resolve_obstacle_collision(std::array<double, 2>& position, std::array<double, 2>& velocity) const -> void
{
    for (const auto& obs : obstacles)
    {
        if (!obs.contains(position[0], position[1])) continue;

        // Push to nearest edge and reflect velocity on that axis
        double dist_left   = position[0] - obs.x_range[0];
        double dist_right  = obs.x_range[1] - position[0];
        double dist_bottom = position[1] - obs.y_range[0];
        double dist_top    = obs.y_range[1] - position[1];
        double min_d = std::min({dist_left, dist_right, dist_bottom, dist_top});

        if      (min_d == dist_left)   { position[0] = obs.x_range[0] - 0.1; velocity[0] = -std::abs(velocity[0]); }
        else if (min_d == dist_right)  { position[0] = obs.x_range[1] + 0.1; velocity[0] =  std::abs(velocity[0]); }
        else if (min_d == dist_bottom) { position[1] = obs.y_range[0] - 0.1; velocity[1] = -std::abs(velocity[1]); }
        else                           { position[1] = obs.y_range[1] + 0.1; velocity[1] =  std::abs(velocity[1]); }
    }
}