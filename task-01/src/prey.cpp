#include <fmt/format.h>
#include "prey.h"
#include "predator.h"
#include <cmath>
#include <algorithm>

void Prey::update(const double time_delta, const Environment& environment,
                   const Predator& predator, const double time,
                   const std::vector<Prey>& all_preys)
{
    time_alive += time_delta;

    // Squared-distance helpers (avoids sqrt in tight loops)
    auto sq = [](double v) { return v * v; };

    double dpx = predator.position[0] - position[0];
    double dpy = predator.position[1] - position[1];
    double dist_to_pred = std::hypot(dpx, dpy);

    double dtx = 90.0 - position[0];
    double dty = 50.0 - position[1];
    double dist_to_target = std::hypot(dtx, dty);

    // ── State machine ────────────────────────────────────────────────────────
    if (dist_to_pred < 7.5)
        state = 1;   // Fleeing
    else if (time > 1600.0)
        state = 2;   // Escaping toward pod
    else
        state = 0;   // Normal

    // ── Behaviour per state ──────────────────────────────────────────────────
    if (state == 1)
    {
        // Run directly away from predator
        if (dist_to_pred > 0)
        {
            velocity[0] = -(dpx / dist_to_pred) * speed * 6.0 * time_delta;
            velocity[1] = -(dpy / dist_to_pred) * speed * 6.0 * time_delta;
        }
    }
    else if (state == 2)
    {
        // Head for escape pod
        if (dist_to_target > 10.0)
        {
            velocity[0] = (dtx / dist_to_target) * speed * 3.0 * time_delta;
            velocity[1] = (dty / dist_to_target) * speed * 3.0 * time_delta;
        }
    }
    else
    {
        // ── Boids flocking (separation + cohesion) ───────────────────────────
        const double SEP_R2 = 4.0  * 4.0;
        const double COH_R2 = 15.0 * 15.0;

        double sep_x = 0, sep_y = 0;
        double coh_x = 0, coh_y = 0;
        int    neighbors = 0;

        for (const auto& other : all_preys)
        {
            if (&other == this) continue;
            double dx = other.position[0] - position[0];
            double dy = other.position[1] - position[1];
            double d2 = sq(dx) + sq(dy);

            if (d2 > COH_R2) continue;   // quick reject

            coh_x += other.position[0];
            coh_y += other.position[1];
            ++neighbors;

            if (d2 < SEP_R2 && d2 > 0)
            {
                double d = std::sqrt(d2);
                sep_x -= dx / d;
                sep_y -= dy / d;
            }
        }

        double new_vx = velocity[0] + sep_x * 0.04 * time_delta;
        double new_vy = velocity[1] + sep_y * 0.04 * time_delta;

        if (neighbors > 0)
        {
            new_vx += (coh_x / neighbors - position[0]) * 0.001 * time_delta;
            new_vy += (coh_y / neighbors - position[1]) * 0.001 * time_delta;
        }

        // Clamp to max speed
        double vmag = std::hypot(new_vx, new_vy);
        if (vmag > speed && vmag > 0) { new_vx = new_vx / vmag * speed; new_vy = new_vy / vmag * speed; }

        velocity[0] = new_vx;
        velocity[1] = new_vy;
    }

    position[0] += 0.5 * velocity[0] * time_delta;
    position[1] += 0.5 * velocity[1] * time_delta;

    environment.restrict_position(position);
    environment.reflect(position, velocity);
    environment.resolve_obstacle_collision(position, velocity);
}

auto Prey::log(const double time) -> void
{
    fmt::print("{:.2f},{:.2f},{:.2f}\n", time, position[0], position[1]);
}