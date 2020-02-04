#pragma once

struct statistic {
    double bytes = 0.0;

    statistic& operator+=(const statistic& b) {
        bytes += b.bytes;
        return *this;
    }

    statistic& operator/=(const double s) {
        bytes /= s;
        return *this;
    }
};
