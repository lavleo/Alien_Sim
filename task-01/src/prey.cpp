#include <fmt/format.h>
#include "prey.h"

void Prey::update(const double time_delta, const Environment& environment, const Predator& predator, const double time) {
    position[0] += 0.5 * velocity[0] * time_delta;
    position[1] += 0.5 * velocity[1] * time_delta;

    double distance_to_predator = std::hypot(predator.position[0] - position[0], predator.position[1] - position[1]);
    double distance_to_target = std::hypot(90 - position[0], 50 - position[1]);

    if (distance_to_predator < 7.5) {
        if (distance_to_predator > 0) {
            velocity[0] = -(predator.position[0] - position[0]) / distance_to_predator * 6 * time_delta;
            velocity[1] = -(predator.position[1] - position[1]) / distance_to_predator * 6 * time_delta;
        }
    }
    else {
        if (time > 1600) {
            if (distance_to_target > 10) {
                velocity[0] = (90 - position[0]) / distance_to_target * 3 * time_delta;
                velocity[1] = (50 - position[1]) / distance_to_target * 3 * time_delta;
            }
        }
    }

    environment.restrict_position(position);
    environment.reflect(position, velocity);
}


auto Prey::log(const double time) -> void
{
    fmt::print("{:.2f},{:.2f},{:.2f}\n", time, position[0], position[1]);
}