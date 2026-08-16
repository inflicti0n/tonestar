#pragma once

#include <array>

struct FieldSpectrum
{
    static constexpr int bins = 64;
    std::array<float, bins> mag {};
};
