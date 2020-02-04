#include <aether/common/math_utils.hh>
//
//#if defined(_MSC_VER)
//
//#include <intrin.h>
//
//uint64_t __builtin_clzll(uint64_t mask) {
//    unsigned long tmp;
//    _BitScanReverse64(&tmp, mask);
//    return tmp;
//}
//
//uint64_t __builtin_ctzll(uint64_t mask) {
//    unsigned long tmp;
//    _BitScanForward64(&tmp, mask);
//    return tmp;
//}
//
//uint64_t __builtin_popcountll(uint64_t value) {
//    return __popcnt64(value);
//}
//#endif