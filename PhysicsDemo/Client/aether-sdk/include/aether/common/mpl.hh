#pragma once

#include <tuple>
#include <cstddef>
#include <climits>

namespace aether {

namespace mpl {

template<typename L, typename T>
struct index_of {
};

template<typename T>
struct index_of<std::tuple<>, T> {
    static_assert(sizeof(T) == 0, "index_of: type not present in list");
};

template<typename T, typename... Ls>
struct index_of<std::tuple<T, Ls...>, T> {
    static constexpr size_t value = 0;
    using value_type = std::integral_constant<size_t, value>;
};

template<typename T, typename L, typename... Ls>
struct index_of<std::tuple<L, Ls...>, T> {
    static constexpr size_t value = 1 + index_of<std::tuple<Ls...>, T>::value;
    using value_type = std::integral_constant<size_t, value>;
};

template<typename L, typename T>
struct contains {
};

template<typename T>
struct contains<std::tuple<>, T> {
    static constexpr bool value = false;
    using value_type = std::integral_constant<bool, value>;
};

template<typename T, typename L, typename... Ls>
struct contains<std::tuple<L, Ls...>, T> {
    static constexpr bool value = contains<std::tuple<Ls...>, T>::value;
    using value_type = std::integral_constant<bool, value>;
};

template<typename T, typename... Ls>
struct contains<std::tuple<T, Ls...>, T> {
    static constexpr bool value = true;
    using value_type = std::integral_constant<bool, value>;
};

template<typename A, typename B>
struct is_superset_of {
};

template<typename B, typename A, typename ...As>
struct is_superset_of<B, std::tuple<A, As...>> {
    static constexpr bool value = contains<B, A>::value && is_superset_of<B, std::tuple<As...>>::value;
    using value_type = std::integral_constant<bool, value>;
};

template<typename ...B>
struct is_superset_of<std::tuple<B...>, std::tuple<>> {
    static constexpr bool value = true;
    using value_type = std::integral_constant<bool, value>;
};

template<typename T, typename L>
struct cons {
};

template<typename T, typename... Ls>
struct cons<T, std::tuple<Ls...>> {
    using value_type = std::tuple<T, Ls...>;
};

template<typename A, typename B>
class subtract {
};

template<typename ...Bs>
class subtract<std::tuple<>, std::tuple<Bs...>> {
    using value_type = std::tuple<>;
};

template<typename A, typename ...As, typename ...Bs>
class subtract<std::tuple<A, As...>, std::tuple<Bs...>> {
    using tail_type = typename subtract<std::tuple<As...>, std::tuple<Bs...>>::value_type;
    typedef std::conditional_t<contains<std::tuple<Bs...>, A>::value,
        tail_type,
        typename cons<A, tail_type>::value_type
    > value_type;
};

template<typename A, typename B>
struct concat {
};

template<typename ...Bs>
struct concat<std::tuple<>, std::tuple<Bs...>> {
    using value_type = std::tuple<Bs...>;
};

template<typename A, typename ...As, typename ...Bs>
struct concat<std::tuple<A, As...>, std::tuple<Bs...>> {
    using value_type = typename cons<A, typename concat<std::tuple<As...>, std::tuple<Bs...>>::value_type>::value_type;
};

template<typename T>
struct size {
};

template<typename... Ls>
struct size<std::tuple<Ls...>> {
    static constexpr size_t value = sizeof...(Ls);
};

template<template<typename> class F, typename L>
struct map {
};

template<template<typename> class F>
struct map<F, std::tuple<>> {
    using value_type = std::tuple<>;
};

template<template<typename> class F, typename L, typename... Ls>
struct map<F, std::tuple<L, Ls...>> {
    using value_type = typename cons<typename F<L>::value_type, typename map<F, std::tuple<Ls...>>::value_type>::value_type;
};

template<typename A, typename B>
struct zip {
};

template<>
struct zip<std::tuple<>, std::tuple<>> {
    using value_type = std::tuple<>;
};

template<typename A, typename B, typename ...As, typename ...Bs>
struct zip<std::tuple<A, As...>, std::tuple<B, Bs...>> {
    using value_type = typename aether::mpl::cons<std::pair<A, B>, typename zip<std::tuple<As...>, std::tuple<Bs...>>::value_type>::value_type;
};

template<typename F, ssize_t Index, typename... Ls>
struct for_each_helper {
    void operator()(const F& func, std::tuple<Ls...>& values) {
        for_each_helper<F, Index - 1, Ls...>()(func, values);
        func(std::get<Index - 1>(values));
    }
};

template<typename F, typename... Ls>
struct for_each_helper<F, 0, Ls...> {
    void operator()(const F& func, std::tuple<Ls...>& values) {
    }
};

template<typename F, typename... Ls>
void for_each(const F& func, std::tuple<Ls...>& values) {
    for_each_helper<F, size<std::tuple<Ls...>>::value, Ls...>()(func, values);
}

template<typename F, size_t Index, typename... Ls>
struct for_each_enumerated_helper {
    void operator()(const F& func, std::tuple<Ls...>& values) {
        for_each_enumerated_helper<F, Index - 1, Ls...>()(func, values);
        func(std::integral_constant<size_t, Index-1>(), std::get<Index - 1>(values));
    }
};

template<typename F, typename... Ls>
struct for_each_enumerated_helper<F, 0, Ls...> {
    void operator()(const F& func, std::tuple<Ls...>& values) {
    }
};

template<typename F, typename... Ls>
void for_each_enumerated(const F& func, std::tuple<Ls...>& values) {
    for_each_enumerated_helper<F, size<std::tuple<Ls...>>::value, Ls...>()(func, values);
}

template<size_t Index, typename L>
struct at {
};

template<size_t Index, typename... Ls>
struct at<Index, std::tuple<Ls...>> {
    using value_type = std::tuple_element_t<Index, std::tuple<Ls...>>;
};

template<size_t Width, typename = void>
struct uint_least {
};

template<size_t Width>
struct uint_least<Width, std::enable_if_t<(Width <= 8)>> {
    using value_type = uint8_t;
};

template<size_t Width>
struct uint_least<Width, std::enable_if_t<(Width > 8 && Width <= 16)>> {
    using value_type = uint16_t;
};

template<size_t Width>
struct uint_least<Width, std::enable_if_t<(Width > 16 && Width <= 32)>> {
    using value_type = uint32_t;
};

template<size_t Width>
struct uint_least<Width, std::enable_if_t<(Width > 32 && Width <= 64)>> {
    using value_type = uint64_t;
};

template<typename ...Ts>
struct maybe {
};

template<typename T>
struct gather_required {
};

template<>
struct gather_required<std::tuple<>> {
    using value_type = std::tuple<>;
};

template<typename T, typename ...Ts>
struct gather_required<std::tuple<T, Ts...>> {
    using value_type = typename cons<T, typename gather_required<std::tuple<Ts...>>::value_type>::value_type;
};


template<typename ...Ms, typename ...Ts>
struct gather_required<std::tuple<maybe<Ms...>, Ts...>> {
    using value_type = typename gather_required<std::tuple<Ts...>>::value_type;
};

template<typename T>
struct gather_optional {
};

template<>
struct gather_optional<std::tuple<>> {
    using value_type = std::tuple<>;
};

template<typename ...Ts>
struct gather_optional<std::tuple<maybe<Ts...>>> {
    using value_type = std::tuple<Ts...>;
};

template<typename T, typename ...Ts>
struct gather_optional<std::tuple<T, Ts...>> {
    using value_type = typename gather_optional<std::tuple<Ts...>>::value_type;
};

template<typename ...Ms, typename T, typename ...Ts>
struct gather_optional<std::tuple<maybe<Ms...>, T, Ts...>> {
    using value_type = typename gather_optional<std::tuple<maybe<Ms...>, Ts...>>::value_type;
};

template<typename ...Ms, typename ...Ns, typename ...Ts>
struct gather_optional<std::tuple<maybe<Ms...>, maybe<Ns...>, Ts...>> {
    using value_type = typename gather_optional<std::tuple<maybe<Ms...,Ns...>, Ts...>>::value_type;
};

template<typename T>
struct gather_all {
};

template<typename ...Ts>
struct gather_all<std::tuple<Ts...>> {
    using required = typename gather_required<std::tuple<Ts...>>::value_type;
    using optional = typename gather_optional<std::tuple<Ts...>>::value_type;
    using value_type = typename concat<required, optional>::value_type;
};

template<typename ...Ts>
struct make_maybe {
};

template<typename ...Ts>
struct make_maybe<std::tuple<Ts...>> {
    using value_type = maybe<Ts...>;
};

template<typename T>
struct add_lvalue_reference {
    using value_type = typename std::add_lvalue_reference<T>::type;
};

}

}
