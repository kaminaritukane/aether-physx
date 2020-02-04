#pragma once

#ifdef WITH_HADEAN_LOGGING

#include <hadean/log/log.hh>
#include <hadean/log/metrics.hh>

namespace aether {
    namespace log = hadean::log;
    namespace metrics = hadean::metrics;
}

#define AETHER_LOGGER(x,y) (HDNLOG(x,y))
#define AETHER_LOG(x) (HDNLOG_DEFAULT(x))
#define AETHER_MKLOGGER(x) (HDNLOG_MKLOGGER(x))

#else

#define AETHER_LOG(x)
#define AETHER_MKLOGGER(x) (x)

#endif
