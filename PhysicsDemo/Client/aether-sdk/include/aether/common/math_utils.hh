#pragma once

#include <cstdint>

#ifndef M_PI
    // not defined on Windows
    #define M_PI 3.14159265358979323846
#endif
//
//#if defined(_MSC_VER)
//uint64_t __builtin_clzll(uint64_t mask);
//uint64_t __builtin_ctzll(uint64_t mask);
//uint64_t __builtin_popcountll(uint64_t value);
//#endif