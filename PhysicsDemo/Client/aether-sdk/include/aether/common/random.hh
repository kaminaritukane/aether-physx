#pragma once

#include <cstdlib>
#include <cstdint>
#include <climits>

template<typename T>
T generate_random_unsigned(unsigned seed = 0) {
    /*
    - seed reinitialization - if there is a seed parameter and > 0,
    - after that the sequence with this seed is produced (no parameter required)
      as long as seed is not reinitialized (call with parameter)
    - default library behaviour is to use srand(1) before the first call to rand()
    */
    if (seed) {
        std::srand(seed);
    }
    T res = 0;
    const size_t random_bits = 15;
    const size_t rounds = (sizeof(T) * CHAR_BIT + random_bits - 1) / random_bits;
    for(size_t i=0; i<rounds; ++i) {
        res <<= random_bits;
        res = res ^ static_cast<T>(rand());
    }
    return res;
}

inline uint32_t generate_random_u32(unsigned seed = 0) {
  return generate_random_unsigned<uint32_t>(seed);
}

inline uint64_t generate_random_u64(unsigned seed = 0) {
  return generate_random_unsigned<uint64_t>(seed);
}

inline float generate_random_f32(unsigned seed = 0) {
  const uint32_t r = generate_random_unsigned<uint32_t>(seed);
  return static_cast<float>(r) / UINT32_MAX;
}

inline double generate_random_f64(unsigned seed = 0) {
  const uint64_t r = generate_random_unsigned<uint64_t>(seed);
  return static_cast<double>(r) / UINT64_MAX;
}
