#include <fmt/format.h>
#include "predator.h"
#include "prey.h"
#include "ship.h"
#include <cmath>
#include <algorithm>

void Predator::update(double time_delta, const Ship& ship,
                      const std::vector<Prey>& prey, double time)
{
    // ── Hatch phase: immobile, wait in the egg ────────────────────────────
    is_hatching = (time < hatch_time);
    if (is_hatching) return;

    // ── Lock-on: nearest non-hiding crew within lock_on_radius ───────────
    const Prey* target = nullptr;
    double      best   = lock_on_radius;
    for (const auto& p : prey) {
        if (p.state == CrewState::HIDING) continue;   // hidden crew are invisible
        double d = std::hypot(p.position[0]-position[0], p.position[1]-position[1]);
        if (d < best) { best = d; target = &p; }
    }

    if (target) {
        double dx = target->position[0] - position[0];
        double dy = target->position[1] - position[1];
        double m  = std::hypot(dx, dy);
        if (m > 0) { velocity[0] = dx/m; velocity[1] = dy/m; }
        locked_on = true;
    } else {
        locked_on = false;
        // Player-supplied velocity direction stays as-is
    }

    // ── Move: speed multiplier grows every 75 crew eaten ─────────────────
    double m = std::hypot(velocity[0], velocity[1]);
    if (m > 0) {
        double speed_mult = std::ceil(eaten / 150.0);
        std::array<double,2> new_pos = {
            position[0] + (velocity[0]/m) * time_delta * base_speed * speed_mult,
            position[1] + (velocity[1]/m) * time_delta * base_speed * speed_mult
        };
        position = ship.resolve_movement(position, new_pos);
    }
}

void Predator::log(double time) {
    fmt::print("{:.2f},{:.2f},{:.2f}\n", time, position[0], position[1]);
}