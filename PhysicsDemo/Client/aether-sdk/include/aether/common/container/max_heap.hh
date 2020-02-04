#pragma once
#include <utility>
#include <optional>
#include <cassert>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <functional>

namespace aether {

namespace container {

template<typename I, typename P>
class max_heap {
private:
    using item_type = I;
    using priority_type = P;
    using value_type = std::tuple<priority_type, item_type, size_t*>;

    std::vector<value_type> values;
    std::unordered_map<item_type, size_t> items;

    void swap(const size_t i1, const size_t i2) {
        assert(i1 < values.size() && i2 < values.size());
        if (i1 == i2) { return; }
        auto &v1 = values[i1];
        auto &v2 = values[i2];
        std::swap(v1, v2);
        // References to the values of an unordered_map remain
        // valid so we can update indices without
        // having to do look ups :)
        std::swap(*std::get<2>(v1), *std::get<2>(v2));
    }

    void sift_down(const size_t idx) {
        const auto l = left(idx);
        const auto r = right(idx);
        auto largest = idx;
        if (l < values.size() && values[l] > values[idx]) {
            largest = l;
        }
        if (r < values.size() && values[r] > values[largest]) {
            largest = r;
        }
        if (largest != idx) {
            swap(largest, idx);
            sift_down(largest);
        }
    }

    void sift_up(const size_t idx) {
        auto i = idx;
        while(i != 0 && values[parent(i)] < values[i]) {
            swap(i, parent(i));
            i = parent(i);
        }
    }

    static size_t parent(const size_t i) {
        return (i - 1) / 2;
    }

    static size_t left(const size_t i) {
        return 2 * i + 1;
    }

    static size_t right(const size_t i) {
        return 2 * i + 2;
    }

    void remove(size_t idx) {
        assert(idx < values.size());
        while(idx != 0) {
            const auto parent_idx = parent(idx);
            swap(idx, parent_idx);
            idx = parent(idx);
        }
        pop();
    }

public:
    max_heap() = default;

    void push(const item_type &item, const priority_type &priority) {
        const auto existing_iter = items.find(item);
        size_t idx;
        if (existing_iter != items.end()) {
            idx = existing_iter->second;
            auto &entry = values[idx];
            assert(std::get<1>(entry) == item);
            entry = std::make_tuple(priority, item, &existing_iter->second);
        } else {
            idx = values.size();
            const auto insertion = items.insert_or_assign(item, idx);
            const auto value = std::make_tuple(priority, item, &insertion.first->second);
            values.push_back(value);
        }

        if (idx != 0 && std::get<0>(values[parent(idx)]) < std::get<0>(values[idx])) {
            sift_up(idx);
        } else if (idx != values.size() - 1) {
            sift_down(idx);
        }

        assert(values.size() == items.size());
    }

    std::optional<std::pair<const item_type&, const priority_type&>> peek() const {
        const auto begin = values.begin();
        if (begin != values.end()) {
            return { { std::ref(std::get<1>(*begin)), std::ref(std::get<0>(*begin)) } };
        } else {
            return std::nullopt;
        }
    }

    bool contains(const item_type &item) const {
        return items.find(item) != items.end();
    }

    bool empty() const {
        return values.empty();
    }

    size_t size() const {
        return values.size();
    }

    void pop() {
        assert(!values.empty() && "Cannot pop empty heap");
        swap(0, size() - 1);
        const auto removed = items.erase(std::get<1>(values.back()));
        assert(removed == 1);
        values.pop_back();
        if (size() > 1) {
            sift_down(0);
        }
        assert(values.size() == items.size());
    }

    void clear() {
        values.clear();
        items.clear();
        assert(values.size() == items.size());
    }
};

}

}
