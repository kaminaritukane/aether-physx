#pragma once

#include <cmath>
#include "math_utils.hh"

struct hsv {
    float h, s, v;
};

struct colour {
    float r, g, b;

    colour() = default;
    colour(float _r, float _g, float _b) : r(_r), g(_g), b(_b) { }
    colour(const hsv &hsv) {
        const auto c = hsv.v * hsv.s;
        auto h = hsv.h;
        while (h < 0.0) {
            h += 2 * M_PI;
        }
        const auto h_unit = h * 3.0 / M_PI;
        const auto h_int = static_cast<int>(h_unit);
        const auto x = c * (1.0 -  std::abs(std::fmod(h_unit, 2.0) - 1.0));
        const auto m = hsv.v - c;
        float r1 = 0.0, g1 = 0.0, b1 = 0.0;
        switch(h_int) {
            case 0: r1=c; g1=x; break;
            case 1: r1=x; g1=c; break;
            case 2: g1=c; b1=x; break;
            case 3: g1=x; b1=c; break;
            case 4: r1=x; b1=c; break;
            case 5: r1=c; b1=x; break;
            default: break;
        }
        r = r1 + m;
        g = g1 + m;
        b = b1 + m;
    }
};
