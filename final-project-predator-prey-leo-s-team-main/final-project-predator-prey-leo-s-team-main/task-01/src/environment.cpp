#include <algorithm>
#include <cmath>
#include "prey.h"
#include "predator.h"
#include "environment.h"

auto Environment::restrict_position(std::array<double, 2>& position) const->void
{
    position[0] = std::clamp(position[0], x_limit[0], x_limit[1]);
    position[1] = std::clamp(position[1], y_limit[0], y_limit[1]);
}

auto Environment::reflect(const std::array<double, 2>& position, std::array<double, 2>& velocity) const->void
{
    if (position[0] == x_limit[0] || position[0] == x_limit[1])
    {
        velocity[0] *= -1.0;
    }
    if (position[1] == y_limit[0] || position[1] == y_limit[1])
    {
        velocity[1] *= -1.0;
    }
}
