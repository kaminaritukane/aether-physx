#pragma once

#include <cstdint>
#include <cassert>

#include <algorithm>
#include <vector>
#include <tuple>
#include <variant>
#include <type_traits>
#include <ostream>

#include "encoding.hh"
#include "interval.hh"
#include <immintrin.h>

namespace aether {

namespace morton {

//https://en.wikipedia.org/wiki/Linear_octree
//https://geidav.wordpress.com/2014/08/18/advanced-octrees-2-node-representations/
//(see Linear (hashed) Octrees section
template <typename MortonCode, typename T = std::monostate>
struct region {
    using morton_type = MortonCode;
    using interval_type = morton::detail::interval<morton_type, T>;
    std::vector<interval_type> intervals;
    using const_iterator = typename decltype(intervals)::const_iterator;

    const_iterator begin() const {
        return intervals.begin();
    }

    const_iterator end() const {
        return intervals.end();
    }

    template<typename M = std::monostate>
    friend bool operator==(const region& lhs, const region<morton_type, M>& rhs) {
        return lhs.intervals == rhs.intervals;
    }

    friend region operator|(const region& lhs, const region& rhs) {
        region r = lhs;
        r |= rhs;
        return r;
    }

    template<typename M = std::monostate>
    friend region operator&(const region& lhs, const region<morton_type, M>& rhs) {
        auto r = lhs;
        r &= rhs;
        return r;
    }

    template<typename F>
    void merge(const region &rhs, const F f) {
        // When modifying this function, be *very* careful about invalidated
        // references after pushing values to intervals. Also note that
        // lhs may be the same object as rhs.
        auto &lhs = *this;
        if (rhs.intervals.empty()) { return; }
        assert(std::is_sorted(lhs.intervals.begin(), lhs.intervals.end()));
        assert(std::is_sorted(rhs.intervals.begin(), rhs.intervals.end()));
        const size_t num_left = lhs.intervals.size();
        const size_t num_right = rhs.intervals.size();
        size_t l_index = 0;
        size_t r_index = 0;
        auto rhs_interval = rhs.intervals[r_index];
        const auto increment_left = [&l_index]() { ++l_index; };
        const auto increment_right = [&r_index, &rhs_interval, &rhs]() {
            ++r_index;
            if (r_index < rhs.intervals.size()) {
                rhs_interval = rhs.intervals[r_index];
            }
        };

        while(r_index < num_right) {
            // rhs does not overlap
            if (l_index >= num_left || rhs_interval.end < lhs.intervals[l_index].start) {
                assert(l_index <= num_left);
                lhs.intervals.push_back(rhs_interval);
                increment_right();
                continue;
            }

            auto &lhs_interval = lhs.intervals[l_index];
            if (lhs_interval.end < rhs_interval.start) {
                increment_left();
                continue;
            }

            // start of one of the intervals is earlier
            if (lhs_interval.start != rhs_interval.start) {
                const bool left_is_earlier = lhs_interval.start < rhs_interval.start;
                auto &first = left_is_earlier ? lhs_interval : rhs_interval;
                const auto &second = left_is_earlier ? rhs_interval : lhs_interval;
                auto new_interval = first;
                new_interval.end = { second.start.data - 1 };
                first.start = second.start;
                lhs.intervals.push_back(new_interval);
                continue;
            }

            // Both intervals start at same offset
            assert(lhs_interval.start == rhs_interval.start);

            if (lhs_interval.end == rhs_interval.end) {
                f(lhs_interval.data, rhs_interval.data);
                increment_left();
                increment_right();
            } else if (lhs_interval.end < rhs_interval.end) {
                f(lhs_interval.data, rhs_interval.data);
                rhs_interval.start = { lhs_interval.end.data + 1};
                increment_left();
            } else if (lhs_interval.end > rhs_interval.end) {
                auto new_interval = lhs_interval;
                new_interval.end = rhs_interval.end;
                f(new_interval.data, rhs_interval.data);
                lhs_interval.start = { rhs_interval.end.data + 1};
                lhs.intervals.push_back(new_interval);
                increment_right();
            }
        }

        // Merge regions with identical data
        if (lhs.intervals.size() > 1) {
            std::sort(lhs.intervals.begin(), lhs.intervals.end());
            size_t merged_index = 0;
            for(size_t i = 1; i < lhs.intervals.size(); ++i) {
                auto &left = lhs.intervals[merged_index];
                auto &right = lhs.intervals[i];
                if (left.end.data + 1 == right.start.data && left.data == right.data) {
                    left.end = right.end;
                } else {
                    ++merged_index;
                    auto &new_left = lhs.intervals[merged_index];
                    new_left = right;
                }
            }

            const size_t new_size = merged_index + 1;
            // This would be much nicer if std::vector had a truncate function
            assert(new_size <= lhs.intervals.size());
            assert(new_size > 0);
            const auto val = lhs.intervals[0];
            lhs.intervals.resize(new_size, val);
        }
    }

    friend region &operator|=(region& lhs, const region& rhs) {
        const auto overwrite = [](T &left, const T &right) {
            left = right;
        };
        lhs.merge(rhs, overwrite);
        return lhs;
    }

    // this assumes regions contain a sorted list of morton intervals
    template<typename M = std::monostate>
    friend region &operator&=(region& lhs, const region<morton_type, M>& rhs) {
        assert(std::is_sorted(lhs.intervals.begin(), lhs.intervals.end()));
        assert(std::is_sorted(rhs.intervals.begin(), rhs.intervals.end()));
        auto lhs_it = lhs.intervals.begin();
        auto rhs_it = rhs.intervals.begin();
        std::vector<interval_type> out = {};
        while(lhs_it != lhs.intervals.end() && rhs_it != rhs.intervals.end()){
            if (lhs_it->end < rhs_it->start) {
                ++lhs_it;
                continue;
            } else if (rhs_it->end < lhs_it->start) {
                ++rhs_it;
                continue;
            }
            auto s = std::max(lhs_it->start, rhs_it->start);
            auto e = std::min(lhs_it->end, rhs_it->end);
            out.push_back(interval_type{s,e, lhs_it->data});
            if (lhs_it->end < rhs_it->end){
                ++lhs_it;
            } else if(rhs_it->end < lhs_it->end) {
                ++rhs_it;
            } else {
                ++rhs_it;
                ++lhs_it;
            }
        }
        lhs.intervals = out;
        return lhs;
    }

    template<typename M = std::monostate>
    friend region &operator-=(region& lhs, const region<morton_type, M>& rhs) {
        assert(std::is_sorted(lhs.intervals.begin(), lhs.intervals.end()));
        assert(std::is_sorted(rhs.intervals.begin(), rhs.intervals.end()));
        auto lhs_it = lhs.intervals.begin();
        auto rhs_it = rhs.intervals.begin();
        std::vector<interval_type> out = {};

        if (lhs_it == lhs.intervals.end())
            // Already empty, can't make it emptier
            return lhs;

        auto s = lhs_it->start;
        while(lhs_it != lhs.intervals.end() && rhs_it != rhs.intervals.end()){
            if (lhs_it->end < rhs_it->start ){ //if the lhs is entirely behind the rhs push the lhs
                // lhs: |------|
                // rhs:          |--|
                out.push_back(interval_type{s,lhs_it->end, lhs_it->data});
                ++lhs_it;
                if (lhs_it != lhs.intervals.end()) {
                    s = lhs_it->start;
                }
                continue;
            }
            if (s > rhs_it->end){ // if the rhs is entirely behind the lhs, push the rhs
                // lhs:      |---|
                // rhs:|--|
                ++rhs_it;
                continue;
            }
            // we know now that the rhs collides with the lhs
            if (s >= rhs_it->start){
                //      A       |  |     B
                // lhs:  |---|  |or|     |---|
                // rhs:|--|     |  | |-------|
                if (lhs_it->end <= rhs_it->end){ // case B - move onto the next interval
                    ++lhs_it;
                    if (lhs_it != lhs.intervals.end()) {
                        s = lhs_it->start;
                    }
                } else { // case A - split the interval and check the next rhs segment
                    s = rhs_it->end + 1;
                    ++rhs_it;
                }
                continue;
            }
            // s must be < rhs
            //      A     |  |     B
            // lhs:|---|  |or| |---|
            // rhs: |-|   |  |   |----|
            out.push_back({s,rhs_it->start-1,lhs_it->data});
            if (rhs_it->end < lhs_it->end) { // case A
                s = rhs_it->end +1;
                ++rhs_it;
            } else { // case B
                ++lhs_it;
                if (lhs_it != lhs.intervals.end()) {
                    s = lhs_it->start;
                }
            }
        }
        if (lhs_it != lhs.intervals.end()) {
            //push the last one, then copy the rest
            out.push_back({s,lhs_it->end, lhs_it->data});
            out.insert(out.end(), ++lhs_it, lhs.intervals.end());
        }
        lhs.intervals = out;
        return lhs;
    }

    template<typename M = std::monostate>
    friend region operator-(const region& lhs, const region<morton_type, M>& rhs) {
        auto x = lhs;
        x -= rhs;
        return x;
    }

    template<typename M>
    bool intersects(const region<morton_type, M>& rhs) const;
    bool empty() const;
    uint64_t area() const;
    bool contains(const morton_type &c) const {
        for (auto& i: intervals) {
            if (c < i.start) {
                return false;
            } else if (c <= i.end) {
                return true;
            }
        }
        return false;
    };
    std::vector<detail::interval<morton_type>> to_cells() const;
    std::vector<detail::interval<morton_type>> to_cells(size_t max_level) const;
    std::vector<std::pair<uint64_t,uint64_t>> count_cells() const;
};

    template<typename MortonCode, typename T>
    template<typename M>
    bool region<MortonCode, T>::intersects(const region<morton_type, M>& rhs) const {
        assert(std::is_sorted(intervals.begin(), intervals.end()) && std::is_sorted(rhs.intervals.begin(), rhs.intervals.end()));
        auto lhs_it = intervals.begin();
        auto rhs_it = rhs.intervals.begin();
        while(lhs_it != intervals.end() && rhs_it != rhs.intervals.end()){
            if (lhs_it->end < rhs_it->start) {
                ++lhs_it;
                continue;
            } else if (rhs_it->end < lhs_it->start) {
                ++rhs_it;
                continue;
            }
            return true;
        }
        return false;
    }

    template<typename MortonCode, typename T>
    bool region<MortonCode, T>::empty() const {
        return intervals.empty();
    }

    template<typename MortonCode, typename T>
    uint64_t region<MortonCode, T>::area() const {
        uint64_t a = 0;
        for(auto &cell : intervals){
            a += cell.area();
        }
        return a;
    }

    template<typename MortonCode, typename T>
    std::vector<detail::interval<MortonCode>> region<MortonCode, T>::to_cells() const {
        std::vector<detail::interval<morton_type>> v = {};
        for (auto &i : intervals){
            auto c = i.to_cells();
            v.insert(v.end(), c.begin(), c.end());
        }
        return v;
    }

    template<typename MortonCode, typename T>
    std::vector<detail::interval<MortonCode>> region<MortonCode, T>::to_cells(size_t max_level) const {
        std::vector<detail::interval<morton_type>> v = {};
        for (auto &i : intervals){
            auto c = i.to_cells(max_level);
            v.insert(v.end(), c.begin(), c.end());
        }
        return v;
    }

    template<typename MortonCode, typename T>
    std::vector<std::pair<uint64_t,uint64_t>> region<MortonCode, T>::count_cells() const {
        std::vector<std::pair<uint64_t,uint64_t>> counts = {};
        if (intervals.empty()) { return counts;}
        counts = intervals[0].count_cells();
        for(size_t i = 1; i < intervals.size(); i++){
            auto v = intervals[i].count_cells();
            for(auto it = v.begin(); it != v.end(); ++it){
                size_t j = 0;
                for(; j < counts.size(); j++){
                    if(counts[j].first < it->first && j < counts.size() -1 && counts[j+1].first > it->first){
                        //insert if between two existing values
                        counts.emplace(counts.begin()+j,*it);
                        break;
                    }
                    if(counts[j].first == it->first){
                        counts[j].second += it->second;
                        break;
                    }
                }
                if (j == counts.size()){
                    counts.push_back(*it);
                }
            }
        }
        return counts;
    }

    template<typename MortonCode, typename T>
    std::ostream &operator<<(std::ostream &o, const region<MortonCode, T> &r) {
        o << "region([";
        for(size_t i = 0; i < r.intervals.size(); ++i) {
            const auto &interval = r.intervals[i];
            o << interval;
            if (i + 1 < r.intervals.size()) { o << ", "; }
        }
        o << "])";
        return o;
    }

} //::morton

}//aether
