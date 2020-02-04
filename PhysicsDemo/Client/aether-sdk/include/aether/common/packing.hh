#pragma once

#if defined(__GNUC__)
#define HADEAN_PACK( ... ) __VA_ARGS__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define HADEAN_PACK( ... ) __pragma( pack(push, 1) ) __VA_ARGS__ __pragma( pack(pop) )
#endif


