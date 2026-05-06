#include "model.h"
#include <algorithm>
#include <fstream>
#include <fmt/format.h>
#include <cmath>
#include <limits>
#include <unordered_map>

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

    // ── Spatial grid for O(N) boids (cell size == cohesion radius = 15) ─────
    constexpr double CELL = 15.0;
    auto cell_key = [](int cx, int cy) -> int64_t {
        return static_cast<int64_t>(cx + 2000) * 100000 + (cy + 2000);
    };
    std::unordered_map<int64_t, std::vector<std::size_t>> grid;
    grid.reserve(preys.size());
    for (std::size_t i = 0; i < preys.size(); ++i) {
        int cx = static_cast<int>(std::floor(preys[i].position[0] / CELL));
        int cy = static_cast<int>(std::floor(preys[i].position[1] / CELL));
        grid[cell_key(cx, cy)].push_back(i);
    }

    // ── Move all prey ────────────────────────────────────────────────────────
    std::vector<const Prey*> neighbors;
    neighbors.reserve(64);

    for (std::size_t i = 0; i < preys.size(); ++i)
    {
        auto& prey = preys[i];

        // Nearest predator using squared distance (avoids sqrt)
        const Predator* nearest = &predators[0];
        double min_d2 = std::numeric_limits<double>::max();
        for (const auto& pred : predators)
        {
            double dx = prey.position[0] - pred.position[0];
            double dy = prey.position[1] - pred.position[1];
            double d2 = dx*dx + dy*dy;
            if (d2 < min_d2) { min_d2 = d2; nearest = &pred; }
        }

        // Gather neighbors from 3×3 grid cells (excludes self)
        neighbors.clear();
        int cx = static_cast<int>(std::floor(prey.position[0] / CELL));
        int cy = static_cast<int>(std::floor(prey.position[1] / CELL));
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                auto it = grid.find(cell_key(cx + dx, cy + dy));
                if (it != grid.end())
                    for (std::size_t j : it->second)
                        if (j != i)
                            neighbors.push_back(&preys[j]);
            }

        prey.update(time_delta, environment, *nearest, time, neighbors);
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
            std::swap(*it, preys.back());
            preys.pop_back();
            continue;
        }

        // Check escaped through pod
        if (std::hypot(prey.position[0] - 90.0, prey.position[1] - 50.0) < 10.0 && time > 1600.0)
        {
            ++escaped;
            std::swap(*it, preys.back());
            preys.pop_back();
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