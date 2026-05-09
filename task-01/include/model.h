#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "prey.h"
#include "predator.h"
#include "environment.h"
#include "ship.h"

using json = nlohmann::json;

// ── Ping: expanding alert ring emitted when a crew member first panics ────────
struct Ping {
    double x{}, y{}, age{0.0};
    static constexpr double MAX_AGE = 3.0;
};

// ── Score ─────────────────────────────────────────────────────────────────────
struct Score {
    int    crew_caught {0};
    int    crew_escaped{0};
    double elapsed_time{0.0};
};

// ── Model ─────────────────────────────────────────────────────────────────────
struct Model {
    Model(std::filesystem::path configuration_file);
    void initialize(std::filesystem::path configuration_file);
    void update(double time_delta);
    void finalize();

    double      time{};
    json        configuration{};
    Environment environment{};   // kept for API compat; ship handles actual collision
    Predator    predator{};
    std::vector<Prey> preys{};
    Ship        ship{};
    std::vector<Ping> pings{};
    Score       score{};
    bool        shuttle_open{false};
};
