#pragma once

#include <filesystem>
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

    void update(const double time_delta);

    void finalize();

    double time{};
    Environment environment{};
    json configuration{};
    Predator predator{};
    std::vector<Prey> preys{};
};
