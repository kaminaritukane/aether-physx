#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <basetsd.h>
using ssize_t = SSIZE_T;
#endif

#include <cstdarg>
#include "packing.hh"
