#include <cstdint>
#include <Eigen/Dense>

namespace aether {
struct HexCoord {
    int64_t q, r, s;
    HexCoord(int64_t q_, int64_t r_, int64_t s_): q(q_), r(r_), s(s_) {
        assert(q + r + s == 0);
    }
    HexCoord& operator+=(const HexCoord& rhs) {
        q += rhs.q;
        r += rhs.r;
        s += rhs.s;
        return *this;
    }
};

const std::vector<HexCoord> directions = {
    HexCoord(1, 0, -1),
    HexCoord(1, -1, 0),
    HexCoord(0, -1, 1),
    HexCoord(-1, 0, 1),
    HexCoord(-1, 1, 0),
    HexCoord(0, 1, -1),
};
std::vector<HexCoord> hex_spiral(size_t radius) {
    HexCoord current {0, 0, 0};
    std::vector<HexCoord> output{current};
    for (size_t r = 1; r < radius; r++) {
        current += directions[4];
        for (size_t s = 0; s < 6; s++) {
            for (size_t l = 0; l < r; l++) {
                output.push_back(current);
                current += directions[s];
            }
        }
    }
    return output;
}

Eigen::Vector2f hex_to_square(const HexCoord& h) {
    float f0 = 3.0 / 2.0;
    float f1 = 0.0;
    float f2 = sqrt(3.0) / 2.0;
    float f3 = sqrt(3.0);
    return Eigen::Vector2f(
        f0 * h.q + f1 * h.r,
        f2 * h.q + f3 * h.r
    );
}

HexCoord square_to_hex(const Eigen::Vector2f& p) {
    float b0 = 2.0 / 3.0;
    float b1 = 0.0;
    float b2 = -1.0 / 3.0;
    float b3 = sqrt(3.0) / 3.0;
    return HexCoord{
        static_cast<int64_t>(b0 * p.x() + b1 * p.y()),
        static_cast<int64_t>(b2 * p.x() + b3 * p.y()),
        static_cast<int64_t>(-(b0 * p.x() + b1 * p.y() + b2 * p.x() + b3 * p.y())),
    };
}
}
