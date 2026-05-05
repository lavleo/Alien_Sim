#pragma once
#include <filesystem>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "prey.h"
#include "predator.h"
#include "environment.h"

using json = nlohmann::json;

struct Model
{
    Model(std::filesystem::path configuration_file);
    void initialize(std::filesystem::path configuration_file);
    void reset();
    void update(const double time_delta);
    void finalize();

    double               time{};
    Environment          environment{};
    json                 configuration{};
    std::vector<Predator> predators{};
    std::vector<Prey>    preys{};
    int                  escaped = 0;

    bool                 enable_flocking     = true;
    bool                 reproduction_enabled = true;

    std::mt19937         rng{};
    std::filesystem::path config_path{};
};