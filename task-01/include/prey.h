#pragma once
#include <array>
#include <deque>
#include <random>
#include <vector>
#include "predator.h"   // predator.h uses only forward-decls, no circularity

class Ship;

// ── Crew state machine ────────────────────────────────────────────────────────
enum class CrewState : int {
    IDLE     = 0,   // wandering in current room
    ALERTED  = 1,   // spotted xenomorph, edging away
    FLEEING  = 2,   // sprinting to nearest vent
    HIDING   = 3,   // inside vent — invisible to lock-on
    ESCAPING = 4,   // shuttle is open, running for it
};

struct Prey
{
    void update(double time_delta, const Ship& ship,
                const Predator& predator, double time);
    void log(double time);

    std::array<double,2> position{};
    std::array<double,2> velocity{};
    CrewState state{CrewState::IDLE};

    // Pathfinding state
    int target_room   {-1};
    std::vector<std::array<double,2>> waypoints;
    int waypoint_idx  {0};

    // O2 trail (last TRAIL_LEN positions, oldest first)
    static constexpr int TRAIL_LEN = 8;
    std::deque<std::array<double,2>> trail;
    double trail_timer{0.0};

    // Timers
    double alert_timer {0.0};
    double wander_timer{0.0};

    // Set true for exactly one tick when the crew member first panics
    bool just_alerted{false};

    // Per-member RNG for idle wandering (seeded deterministically in model.cpp)
    std::mt19937 wander_rng{42};

private:
    void update_state       (const Ship& ship, const Predator& predator, double time);
    void move_toward_waypoint(const Ship& ship, double speed, double time_delta);
    void set_path_to        (const Ship& ship, int room_id);
};
