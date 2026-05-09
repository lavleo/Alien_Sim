#include <fmt/format.h>
#include "prey.h"
#include "ship.h"
#include <cmath>
#include <algorithm>

// ── Tuning constants ──────────────────────────────────────────────────────────
static constexpr double DETECT_RADIUS  = 18.0;   // crew spots predator at this dist
static constexpr double PANIC_RADIUS   = 30.0;   // flee-cool-off zone boundary
static constexpr double CREW_SPEED     =  7.0;   // base units / second
static constexpr double ALERT_COOLDOWN =  6.0;   // seconds before alarm fades

// ── Path helpers ──────────────────────────────────────────────────────────────
void Prey::set_path_to(const Ship& ship, int room_id) {
    // Skip recompute if already heading there
    if (room_id == target_room && waypoint_idx < (int)waypoints.size()) return;
    target_room  = room_id;
    int from     = ship.room_at(position[0], position[1]);
    if (from < 0) from = ship.nearest_room(position[0], position[1]);
    waypoints    = ship.path_between_rooms(from, room_id);
    waypoint_idx = 0;
}

void Prey::move_toward_waypoint(const Ship& ship, double speed, double time_delta) {
    if (waypoints.empty() || waypoint_idx >= (int)waypoints.size()) return;
    const auto& wp = waypoints[waypoint_idx];
    double dx   = wp[0] - position[0];
    double dy   = wp[1] - position[1];
    double dist = std::hypot(dx, dy);
    if (dist < 1.5) { ++waypoint_idx; return; }
    double step  = speed * time_delta;
    auto new_pos = std::array<double,2>{
        position[0] + (dx/dist) * step,
        position[1] + (dy/dist) * step };
    position   = ship.resolve_movement(position, new_pos);
    velocity[0] = dx/dist;
    velocity[1] = dy/dist;
}

// ── State machine ─────────────────────────────────────────────────────────────
void Prey::update_state(const Ship& ship, const Predator& predator, double time) {
    just_alerted = false;

    // Crew can't detect the xenomorph while it's still in the egg
    if (predator.is_hatching) return;

    double dist = std::hypot(predator.position[0]-position[0],
                             predator.position[1]-position[1]);

    // Shuttle opens at t=1600 — override everything else
    if (time >= 1600.0 && state != CrewState::ESCAPING) {
        state = CrewState::ESCAPING;
        set_path_to(ship, ship.shuttle_room_id);
        return;
    }

    switch (state) {

    case CrewState::IDLE:
        if (dist < DETECT_RADIUS) {
            state        = CrewState::ALERTED;
            alert_timer  = ALERT_COOLDOWN;
            just_alerted = true;   // triggers an alert ping in model.cpp
        }
        break;

    case CrewState::ALERTED:
        alert_timer -= 0.25;
        if (dist < DETECT_RADIUS) {
            // Upgrade: full sprint to best vent
            alert_timer = ALERT_COOLDOWN;
            state       = CrewState::FLEEING;

            // Choose vent farthest from the predator
            int    best_vent  = -1;
            double best_score = -1e9;
            for (int vid : ship.vent_room_ids) {
                auto [vx, vy] = ship.room_center(vid);
                double d = std::hypot(vx-predator.position[0], vy-predator.position[1]);
                if (d > best_score) { best_score = d; best_vent = vid; }
            }
            if (best_vent >= 0) {
                set_path_to(ship, best_vent);
            } else {
                // Fallback: flee toward point directly away from predator
                double fx = position[0]*2.0 - predator.position[0];
                double fy = position[1]*2.0 - predator.position[1];
                set_path_to(ship, ship.nearest_room(fx, fy));
            }
        } else if (alert_timer <= 0.0) {
            state       = CrewState::IDLE;
            waypoints.clear(); waypoint_idx = 0; target_room = -1;
        }
        break;

    case CrewState::FLEEING: {
        alert_timer -= 0.25;
        if (dist < DETECT_RADIUS) alert_timer = ALERT_COOLDOWN;

        bool path_done = (waypoint_idx >= (int)waypoints.size());
        int  cur_room  = ship.room_at(position[0], position[1]);
        bool in_vent   = false;
        for (int vid : ship.vent_room_ids) if (cur_room == vid) in_vent = true;

        if (path_done && in_vent) {
            state = CrewState::HIDING;
            waypoints.clear(); waypoint_idx = 0;
        } else if (alert_timer <= 0.0 && dist > PANIC_RADIUS) {
            state       = CrewState::IDLE;
            waypoints.clear(); waypoint_idx = 0; target_room = -1;
        }
        break;
    }

    case CrewState::HIDING:
        // Emerge once predator is clearly far away
        if (dist > PANIC_RADIUS * 1.5) {
            state       = CrewState::IDLE;
            waypoints.clear(); waypoint_idx = 0; target_room = -1;
        }
        break;

    case CrewState::ESCAPING:
        break;   // stay in this state until model removes the crew member
    }
}

// ── Main update ───────────────────────────────────────────────────────────────
void Prey::update(double time_delta, const Ship& ship,
                  const Predator& predator, double time)
{
    update_state(ship, predator, time);

    // ── O2 trail: record position every 0.4 s ─────────────────────────────
    trail_timer += time_delta;
    if (trail_timer >= 0.4) {
        trail_timer = 0.0;
        trail.push_back(position);
        if ((int)trail.size() > TRAIL_LEN) trail.pop_front();
    }

    // ── Movement by state ─────────────────────────────────────────────────
    switch (state) {

    case CrewState::IDLE: {
        // Pick a new wander target when the last one is reached
        if (wander_timer <= 0.0 || waypoints.empty()
            || waypoint_idx >= (int)waypoints.size())
        {
            wander_timer = std::uniform_real_distribution<>(1.5, 4.0)(wander_rng);
            int cur = ship.room_at(position[0], position[1]);
            if (cur < 0) cur = ship.nearest_room(position[0], position[1]);
            const Room& room = ship.rooms[cur];
            std::uniform_real_distribution<> rx(room.cx - room.hw*0.65,
                                                room.cx + room.hw*0.65);
            std::uniform_real_distribution<> ry(room.cy - room.hh*0.65,
                                                room.cy + room.hh*0.65);
            waypoints    = {{{rx(wander_rng), ry(wander_rng)}}};
            waypoint_idx = 0;
        }
        wander_timer -= time_delta;
        move_toward_waypoint(ship, CREW_SPEED * 0.35, time_delta);
        break;
    }

    case CrewState::ALERTED:
        move_toward_waypoint(ship, CREW_SPEED * 0.65, time_delta);
        break;

    case CrewState::FLEEING:
        move_toward_waypoint(ship, CREW_SPEED * 1.1, time_delta);
        break;

    case CrewState::HIDING:
        velocity = {0.0, 0.0};   // stay perfectly still
        break;

    case CrewState::ESCAPING:
        move_toward_waypoint(ship, CREW_SPEED * 1.3, time_delta);
        break;
    }
}

void Prey::log(double time) {
    fmt::print("{:.2f},{:.2f},{:.2f}\n", time, position[0], position[1]);
}
