#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <type_traits>
#include "random.hh"

#include <cmath>
#include "math_utils.hh"

#define CHECK_NAN(v) {assert(!std::isnan((v)) && !std::isnan((v)));}
#define CHECK_INF(v) {assert(!std::isinf((v)) && !std::isinf((v)));}
#define CHECK_NAN_INF(v) {CHECK_NAN(v);CHECK_INF(v);}

#define CHECK_NAN_2D(v) {assert(!std::isnan((v).x) && !std::isnan((v).y));}
#define CHECK_INF_2D(v) {assert(!std::isinf((v).x) && !std::isinf((v).y));}
#define CHECK_NAN_INF_2D(v) {CHECK_NAN_2D(v);CHECK_INF_2D(v);}

#define CHECK_NAN_3D(v) {assert(!std::isnan((v).x) && !std::isnan((v).y) && !std::isnan((v).z));}
#define CHECK_INF_3D(v) {assert(!std::isinf((v).x) && !std::isinf((v).y) && !std::isinf((v).z));}
#define CHECK_NAN_INF_3D(v) {CHECK_NAN_3D(v);CHECK_INF_3D(v);}


#define EIGEN_CHECK_NAN_3D(v) {assert(!std::isnan((v).x()) && !std::isnan((v).y()) && !std::isnan((v).z()));}
#define EIGEN_CHECK_INF_3D(v) {assert(!std::isinf((v).x()) && !std::isinf((v).y()) && !std::isinf((v).z()));}
#define EIGEN_CHECK_NAN_INF_3D(v) {EIGEN_CHECK_NAN_3D(v);EIGEN_CHECK_INF_3D(v);}

struct vec2f {
    using self_type = vec2f;
    using value_type = float;
    value_type x, y;

    static vec2f zero() {
        vec2f result;
        result.x = 0.0f;
        result.y = 0.0f;
        return result;
    }

    static vec2f components(float _x, float _y) {
        vec2f result;
        result.x = _x;
        result.y = _y;
        return result;
    }

    vec2f &operator*=(const float s) {
        x *= s;
        y *= s;
        return *this;
    }

    vec2f &operator/=(const float s) {
        x /= s;
        y /= s;
        return *this;
    }

    vec2f &operator+=(const vec2f &other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    vec2f &operator-=(const vec2f &other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    self_type operator*(const value_type s) const {
        self_type result(*this);
        result *= s;
        return result;
    }

    self_type operator/(const value_type s) const {
        self_type result(*this);
        result /= s;
        return result;
    }

    self_type operator+(const self_type &other) const {
        self_type result(*this);
        result += other;
        return result;
    }

    self_type operator-(const self_type &other) const {
        self_type result(*this);
        result -= other;
        return result;
    }

    self_type operator-() const {
        self_type result(*this);
        result *= -1;
        return result;
    }

    value_type &operator[](const size_t index) {
        switch (index) {
            case 0: return x;
            case 1: return y;
            default: assert(false && "Out of bounds access");
        }
    }

    const value_type &operator[](const size_t index) const {
        switch (index) {
            case 0: return x;
            case 1: return y;
            default: assert(false && "Out of bounds access");
        }
    }
};

inline vec2f vec2f_new(float x, float y) {
    vec2f v;
    v.x = x;
    v.y = y;
    return v;
}

struct vec3f {
    using self_type = vec3f;
    using value_type = float;
    value_type x, y, z;

    static vec3f zero() {
        vec3f result;
        result.x = 0.0f;
        result.y = 0.0f;
        result.z = 0.0f;
        return result;
    }

    static vec3f components(float _x, float _y, float _z) {
        vec3f result;
        result.x = _x;
        result.y = _y;
        result.z = _z;
        CHECK_NAN_INF_3D(result);
        return result;
    }

    vec3f &operator*=(const float s) {
        x *= s;
        y *= s;
        z *= s;
        CHECK_NAN_INF_3D(*this);
        return *this;
    }

    vec3f &operator/=(const float s) {
        x /= s;
        y /= s;
        z /= s;
        CHECK_NAN_INF_3D(*this);
        return *this;
    }

    vec3f &operator+=(const vec3f &other) {
        x += other.x;
        y += other.y;
        z += other.z;
        CHECK_NAN_INF_3D(*this);
        return *this;
    }

    vec3f &operator-=(const vec3f &other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        CHECK_NAN_INF_3D(*this);
        return *this;
    }

    bool operator==(const self_type &other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const self_type &other) const {
        return !(*this == other);
    }

    self_type operator*(const value_type s) const {
        self_type result(*this);
        result *= s;
        return result;
    }

    self_type operator/(const value_type s) const {
        self_type result(*this);
        result /= s;
        return result;
    }

    self_type operator+(const self_type &other) const {
        self_type result(*this);
        result += other;
        return result;
    }

    self_type operator-(const self_type &other) const {
        self_type result(*this);
        result -= other;
        return result;
    }

    self_type operator-() const {
        self_type result(*this);
        result *= -1;
        return result;
    }

    value_type dot(const vec3f &other) const {
        auto ret = x * other.x + y * other.y + z * other.z;
        CHECK_NAN_INF(ret);
        return ret;
    }

    value_type norm2() const {
        auto ret = sqrtf(this->dot(*this));
        CHECK_NAN_INF(ret);
        return ret;
    }

    void normalize() {
        const auto n = this->norm2();
        assert(n != 0.0f);
        *this /= n;
    }

    self_type normalized() const {
        self_type result(*this);
        result.normalize();
        return result;
    }

    value_type &operator[](const size_t index) {
        switch (index) {
            case 0: return x;
            case 1: return y;
            case 2: return z;
            default: assert(false && "Out of bounds access");
        }
    }

    const value_type &operator[](const size_t index) const {
        switch (index) {
            case 0: return x;
            case 1: return y;
            case 2: return z;
            default: assert(false && "Out of bounds access");
        }
    }

    static vec3f uniform_unit() {
        float phi = generate_random_f32()*2.0*M_PI;
        float z = generate_random_f32()*2.0-1.0;
        float sinz = sqrt(1-z*z);
        return {sinz * std::cos(phi), sinz * std::sin(phi), z};
    }

    vec3f() = default;

    vec3f(const vec3f &) = default;

    vec3f(value_type x_, value_type y_, value_type z_) : x(x_), y(y_), z(z_) {};

    inline bool isfinite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
            std::abs(x) < 1e12f && std::abs(y) < 1e12f && std::abs(z) < 1e12f;
    }
};

static_assert(std::is_trivially_copyable<vec2f>::value, "vec2f must be trivially copyable");
static_assert(std::is_trivially_copyable<vec3f>::value, "vec3f must be trivially copyable");

static_assert(std::is_standard_layout<vec2f>::value, "vec2f must has standard layout");
static_assert(std::is_standard_layout<vec3f>::value, "vec3f must has standard layout");

inline std::ostream &operator<<(std::ostream& os, const vec3f &v) {
    std::ostringstream ss;
    ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os << ss.str();
}

inline vec3f vec3f_new(float x, float y, float z) {
    vec3f v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

inline void vec2f_add_scaled(vec2f *y, float alpha, const vec2f *x) {
    y->x += x->x * alpha;
    y->y += x->y * alpha;
}

inline void vec3f_add_scaled(vec3f *y, float alpha, const vec3f *x) {
    y->x += x->x * alpha;
    y->y += x->y * alpha;
    y->z += x->z * alpha;
}

inline float dot(const vec2f &x, const vec2f &y) {
    return x.x * y.x + x.y * y.y;
}

inline float dot(const vec3f &x, const vec3f &y) {
    return x.dot(y);
}

inline float norm2(const vec2f &x) {
    return sqrtf(dot(x,x));
}

inline float norm2(const vec3f &x) {
    return x.norm2();
}

inline void normalize(vec2f &x) {
    const auto n = norm2(x);
    assert(n != 0.0f);
    x /= n;
}

inline void normalize(vec3f &x) {
    x.normalize();
}

inline vec2f normalized(const vec2f &x) {
    auto result(x);
    normalize(result);
    return result;
}

inline vec3f normalized(const vec3f &x) {
    return x.normalized();
}

//Distance between position vectors.
inline float distance(const vec2f &p0, const vec2f &p1) {
    return norm2(p1 - p0);
}

inline float distance(const vec3f &p0, const vec3f &p1) {
    return norm2(p1 - p0);
}
