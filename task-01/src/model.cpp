#include "model.h"
#include <algorithm>
#include <fstream>
#include <fmt/format.h>

Model::Model(std::filesystem::path configuration_file) {
    initialize(configuration_file);
}


void Model::initialize(std::filesystem::path configuration_file)
{
    time = 0.0;

    auto cfs = std::ifstream{ configuration_file };
    configuration = json::parse(cfs);

    auto number_of_preys = configuration["number_of_preys"].get<std::size_t>();
    preys.resize(number_of_preys);

    auto eng = std::mt19937{ 1337 };
    auto dis = std::uniform_real_distribution<>{ -5, 5 };
    std::ranges::generate(preys, [&]() -> Prey {
        return {
            {dis(eng), dis(eng)},
            {dis(eng), dis(eng)}
        };});
    if (time < 20) {
        predator.velocity[0] = 0;
        predator.velocity[1] = 0;
    }

}

void Model::update(const double time_delta) {
    time += time_delta;

    for (auto ite = preys.begin(); ite != preys.end(); ) {
        Prey& prey = *ite;

        prey.update(time_delta, environment, predator, time);

        if (std::hypot(prey.position[0] - predator.position[0], prey.position[1] - predator.position[1]) < 2.5 && time > 50) {
            ite = preys.erase(ite);
            predator.eaten++;
        }
        else if (std::hypot(prey.position[0] - 100, prey.position[1] - 50) < 10 && time > 1600) {
            ite = preys.erase(ite);
        }
        else {
            ++ite;
        }
    }

    predator.update(time_delta, environment, preys);
}


void Model::finalize()
{}