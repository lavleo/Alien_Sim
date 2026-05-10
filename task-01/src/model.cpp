#include "model.h"
#include <algorithm>
#include <fstream>
#include <fmt/format.h>
#include <cmath>
#include <random>

Model::Model(std::filesystem::path configuration_file) {
    initialize(configuration_file);
}

void Model::initialize(std::filesystem::path configuration_file) {
    time  = 0.0;
    score = {};
    pings.clear();

    auto cfs = std::ifstream{configuration_file};
    configuration = json::parse(cfs);

    unsigned seed = configuration.value("seed", 1337u);
    auto     rng  = std::mt19937{seed};

    // ── Generate ship layout ──────────────────────────────────────────────
    ship.generate(rng, 100.0);

    // ── Initialise predator in the centre room (id=4) ─────────────────────
    predator           = Predator{};
    predator.base_speed     = configuration.value("predator_speed", 15.0);
    predator.lock_on_radius = configuration.value("lock_on_radius", 20.0);
    predator.hatch_time     = configuration.value("hatch_time",     20.0);
    predator.is_hatching    = true;
    predator.eaten          = 1.0;
    auto [pcx, pcy]         = ship.room_center(4);
    predator.position       = {pcx, pcy};
    predator.velocity       = {0.0, 0.0};

    // ── Spawn crew in non-special, non-centre rooms ───────────────────────
    int n_preys = configuration.value("number_of_preys", 50);
    preys.clear();
    preys.reserve(n_preys);

    std::vector<int> spawn_rooms;
    for (const auto& r : ship.rooms)
        if (!r.is_shuttle_bay() && r.id != 4)
            spawn_rooms.push_back(r.id);

    std::uniform_int_distribution<int>  room_pick(0, (int)spawn_rooms.size()-1);
    std::uniform_int_distribution<unsigned> seed_dist;

    for (int i = 0; i < n_preys; ++i) {
        Prey p{};
        int rid          = spawn_rooms[room_pick(rng)];
        const Room& room = ship.rooms[rid];
        std::uniform_real_distribution<> rx(room.cx - room.hw*0.7,
                                            room.cx + room.hw*0.7);
        std::uniform_real_distribution<> ry(room.cy - room.hh*0.7,
                                            room.cy + room.hh*0.7);
        p.position   = {rx(rng), ry(rng)};
        p.velocity   = {0.0, 0.0};
        p.state      = CrewState::IDLE;
        p.wander_rng = std::mt19937{seed_dist(rng)};
        preys.push_back(std::move(p));
    }
}

void Model::update(double time_delta) {
    time += time_delta;
    shuttle_open = (time >= 1600.0);

    // ── Age out old pings ─────────────────────────────────────────────────
    for (auto& p : pings) p.age += time_delta;
    pings.erase(std::remove_if(pings.begin(), pings.end(),
        [](const Ping& p){ return p.age >= Ping::MAX_AGE; }), pings.end());

    // ── Update crew ───────────────────────────────────────────────────────
    for (auto it = preys.begin(); it != preys.end(); ) {
        it->update(time_delta, ship, predator, time);

        // Alert ping: one ring emitted the first tick the crew spots the alien
        if (it->just_alerted)
            pings.push_back({it->position[0], it->position[1], 0.0});

        // Caught: xenomorph reaches crew (not while hatching)
        bool caught = !predator.is_hatching &&
            std::hypot(it->position[0]-predator.position[0],
                       it->position[1]-predator.position[1]) < 2.5;

        // Escaped: crew made it into the shuttle bay
        bool escaped = shuttle_open &&
            ship.rooms[ship.shuttle_room_id]
                .contains(it->position[0], it->position[1]);

        if (caught) {
            predator.eaten++;
            score.crew_caught++;
            it = preys.erase(it);
        } else if (escaped) {
            score.crew_escaped++;
            it = preys.erase(it);
        } else {
            ++it;
        }
    }

    // ── Update predator ───────────────────────────────────────────────────
    predator.update(time_delta, ship, preys, time);
    score.elapsed_time = time;
}

void Model::finalize() {}
