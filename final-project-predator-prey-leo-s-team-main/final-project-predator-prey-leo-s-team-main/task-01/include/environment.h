#pragma once

#include <array>

struct Environment
{
    auto restrict_position(std::array<double, 2>& position) const->void;

    auto reflect(const std::array<double, 2>& position, std::array<double, 2>& velocity) const->void;


    std::array<double, 2> x_limit{ -100, 100 };
    std::array<double, 2> y_limit{ -100, 100 };
};