#pragma once
#include <deque>
#include <ctime>
#include <cassert>
#include <algorithm>
#include <aether/common/timer.hh>
#include <stdio.h>

template <typename T>
class statistics {
    static constexpr double interval = 0.5;
    using sample_t = T;
    double history_length;
    std::deque<T> previous;
    aether::timer::time_type start_time;
    uint64_t last_tick;

  public:
    statistics(const double _num_seconds) : history_length(_num_seconds), last_tick(0) {
        assert(history_length > 0.0);
        start_time = aether::timer::get();
        previous.push_front(sample_t());
    }

    statistics &operator+=(const sample_t &sample) {
        const double delta = aether::timer::diff(aether::timer::get(), start_time);
        const uint64_t tick = static_cast<uint64_t>(delta / interval);

        for (; previous.empty() || last_tick < tick; ++last_tick)
            previous.push_front(sample_t());

        previous.front() += sample;
        while (previous.size() > history_length / interval)
            previous.pop_back();

        return *this;
    }

    sample_t get_sample_total(const double duration) {
        assert(duration <= this->history_length);
        size_t num_added = 0;
        double current_duration = 0.0;

        sample_t result;
        while (num_added < previous.size() && current_duration < duration) {
            current_duration += interval;
            result += previous[num_added];
            ++num_added;
        }

        return result;
    }

    sample_t get_sample_per_second(const double num_seconds) {
        sample_t stat = get_sample_total(num_seconds);
        stat /= std::min(num_seconds, previous.size() * interval);
        return stat;
    }
};
