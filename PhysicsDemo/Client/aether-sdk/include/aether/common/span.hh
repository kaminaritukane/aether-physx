#pragma once
#include <type_traits>
#include <cstddef>

namespace aether {

template<typename T>
class span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using index_type = std::size_t;
    using pointer = element_type*;
    using reference = element_type&;
    using iterator = element_type*;

private:
    element_type *the_data = nullptr;
    index_type extent = 0;

public:
    span() = default;

    span(const span&) = default;

    span(pointer _data, index_type _extent) : the_data(_data), extent(_extent) {
    }

    template<class Container>
    span(Container &container) : the_data(container.data()), extent(container.size()) {
    }

    index_type size() const {
        return extent;
    }

    index_type size_bytes() const {
        return size() * sizeof(element_type);
    }

    bool empty() const {
        return extent == 0;
    }

    pointer data() const {
        return the_data;
    }

    reference operator[](const index_type idx) const {
        return the_data[idx];
    }

    reference front() const {
        return *begin();
    }

    reference back() const {
        return *(end() - 1);
    }

    iterator begin() const {
        return the_data;
    }

    iterator end() const {
        return the_data + extent;
    }
};

}
