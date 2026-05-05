#pragma once
#include <array>
#include <vector>

struct Obstacle
{
    std::array<double, 2> x_range;
    std::array<double, 2> y_range;

    bool contains(double x, double y) const
    {
        return x >= x_range[0] && x <= x_range[1] &&
               y >= y_range[0] && y <= y_range[1];
    }
};

struct Environment
{
    auto restrict_position(std::array<double, 2>& position) const -> void;
    auto reflect(const std::array<double, 2>& position, std::array<double, 2>& velocity) const -> void;
    auto resolve_obstacle_collision(std::array<double, 2>& position, std::array<double, 2>& velocity) const -> void;

    std::array<double, 2> x_limit{ -100, 100 };
    std::array<double, 2> y_limit{ -100, 100 };
    std::vector<Obstacle>  obstacles;
};