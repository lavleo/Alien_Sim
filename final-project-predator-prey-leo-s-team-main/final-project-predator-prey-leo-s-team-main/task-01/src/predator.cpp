#include <fmt/format.h>
#include "predator.h"
#include <algorithm>
#include <cmath>
#include<random>
#include <numbers>

auto Predator::update(const double time_delta, const Environment& environment, const std::vector<Prey>& prey) -> void
{
    auto distance_to_predator = [&](const Prey& prey) {
        return std::hypot(prey.position[0] - position[0], prey.position[1] - position[1]);
        };

    auto closest_prey = std::min_element(prey.begin(), prey.end(), [&](const Prey& f1, const Prey& f2) {
        return distance_to_predator(f1) < distance_to_predator(f2);
        });

    if (closest_prey != prey.end()) {
        double dx = closest_prey->position[0] - position[0];
        double dy = closest_prey->position[1] - position[1];

        double magnitude = std::hypot(dx, dy);

        if (magnitude > 0) {
            dx /= magnitude;
            dy /= magnitude;
        }

        velocity[0] = dx;
        velocity[1] = dy;
    }

    position[0] += velocity[0] * time_delta * std::ceil(eaten / 75);
    position[1] += velocity[1] * time_delta * std::ceil(eaten / 75);

    environment.restrict_position(position);

    environment.reflect(position, velocity);

}


auto Predator::log(const double time) -> void
{
    fmt::print("{:.2f},{:.2f},{:.2f}\n", time, position[0], position[1]);
}