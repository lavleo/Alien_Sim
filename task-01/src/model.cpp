#include "model.h"
#include <algorithm>
#include <fstream>
#include <fmt/format.h>
#include <cmath>
#include <limits>

Model::Model(std::filesystem::path configuration_file)
{
    config_path = configuration_file;
    initialize(configuration_file);
}

void Model::initialize(std::filesystem::path configuration_file)
{
    time    = 0.0;
    escaped = 0;

    auto cfs  = std::ifstream{ configuration_file };
    configuration = json::parse(cfs);

    // ── RNG seeded from config (fixes the hardcoded-1337 bug) ────────────────
    auto seed = configuration["seed"].get<unsigned int>();
    rng = std::mt19937{ seed };

    // ── Environment ──────────────────────────────────────────────────────────
    double region = configuration["region_limit"].get<double>();
    environment.x_limit = { -region, region };
    environment.y_limit = { -region, region };

    environment.obstacles.clear();
    if (configuration.contains("obstacles"))
        for (const auto& obs : configuration["obstacles"])
            environment.obstacles.push_back({
                { obs["x"][0].get<double>(), obs["x"][1].get<double>() },
                { obs["y"][0].get<double>(), obs["y"][1].get<double>() }
            });

    // ── Feature flags ────────────────────────────────────────────────────────
    enable_flocking      = configuration.value("enable_flocking",      true);
    reproduction_enabled = configuration.value("reproduction_enabled", true);

    // ── Prey ─────────────────────────────────────────────────────────────────
    auto n_prey    = configuration["number_of_preys"].get<std::size_t>();
    auto prey_spd  = configuration.value("prey_speed", 1.0);
    preys.clear();
    preys.reserve(n_prey);

    auto pos_dis = std::uniform_real_distribution<>{ -region * 0.5, region * 0.5 };
    auto vel_dis = std::uniform_real_distribution<>{ -5.0, 5.0 };

    for (std::size_t i = 0; i < n_prey; ++i)
    {
        Prey p;
        p.position  = { pos_dis(rng), pos_dis(rng) };
        p.velocity  = { vel_dis(rng), vel_dis(rng) };
        p.speed     = prey_spd;
        preys.push_back(p);
    }

    // ── Predators ────────────────────────────────────────────────────────────
    auto n_pred   = configuration.value("number_of_predators", 1);
    auto pred_spd = configuration.value("predator_speed", 15.0);
    predators.clear();

    // Spread predators to different quadrant corners
    std::array<std::array<double,2>, 4> spawn_pts{{
        { 0.0,                0.0                },
        { -region * 0.5,     -region * 0.5      },
        {  region * 0.5,     -region * 0.5      },
        {  0.0,               region * 0.5      }
    }};

    for (int i = 0; i < n_pred; ++i)
    {
        Predator pred;
        pred.position = spawn_pts[i % static_cast<int>(spawn_pts.size())];
        pred.velocity = { 5.0, 3.0 };
        pred.eaten    = 0;
        pred.speed    = pred_spd;
        predators.push_back(pred);
    }
}

void Model::reset()
{
    initialize(config_path);
}

void Model::update(const double time_delta)
{
    time += time_delta;

    if (predators.empty()) return;

    // ── Snapshot for flocking (avoids iteration-while-erasing issues) ────────
    std::vector<Prey> flock_ref;
    if (enable_flocking) flock_ref = preys;

    // ── Move all prey ────────────────────────────────────────────────────────
    for (auto& prey : preys)
    {
        // Each prey flees from the nearest predator
        const Predator* nearest = &predators[0];
        double min_d = std::numeric_limits<double>::max();
        for (const auto& pred : predators)
        {
            double d = std::hypot(prey.position[0] - pred.position[0],
                                  prey.position[1] - pred.position[1]);
            if (d < min_d) { min_d = d; nearest = &pred; }
        }
        prey.update(time_delta, environment, *nearest, time, flock_ref);
    }

    // ── Eating / escaping / reproduction ─────────────────────────────────────
    std::vector<Prey> offspring;
    auto uni = std::uniform_real_distribution<>{ 0.0, 1.0 };
    const std::size_t MAX_PREY = 3000;

    for (auto it = preys.begin(); it != preys.end(); )
    {
        Prey& prey = *it;

        // Check eaten by any predator
        bool eaten = false;
        for (auto& pred : predators)
        {
            if (std::hypot(prey.position[0] - pred.position[0],
                           prey.position[1] - pred.position[1]) < 2.5 && time > 50)
            {
                pred.eaten++;
                eaten = true;
                break;
            }
        }

        if (eaten)
        {
            it = preys.erase(it);
            continue;
        }

        // Check escaped through pod
        if (std::hypot(prey.position[0] - 90.0, prey.position[1] - 50.0) < 10.0 && time > 1600.0)
        {
            ++escaped;
            it = preys.erase(it);
            continue;
        }

        // Reproduction: safe, distant, well-rested prey
        if (reproduction_enabled && preys.size() + offspring.size() < MAX_PREY
            && prey.time_alive > 300.0 && prey.state == 0)
        {
            double min_pred_dist = std::numeric_limits<double>::max();
            for (const auto& pred : predators)
                min_pred_dist = std::min(min_pred_dist,
                    std::hypot(prey.position[0] - pred.position[0],
                               prey.position[1] - pred.position[1]));

            if (min_pred_dist > 25.0 && uni(rng) < 0.0002 * time_delta)
            {
                Prey child;
                child.position  = { prey.position[0] + 1.0, prey.position[1] + 1.0 };
                child.velocity  = { -prey.velocity[0] * 0.5, -prey.velocity[1] * 0.5 };
                child.speed     = prey.speed;
                offspring.push_back(child);
            }
        }
        ++it;
    }

    preys.insert(preys.end(), offspring.begin(), offspring.end());

    // ── Move all predators ───────────────────────────────────────────────────
    for (auto& pred : predators)
        pred.update(time_delta, environment, preys);
}

void Model::finalize() {}