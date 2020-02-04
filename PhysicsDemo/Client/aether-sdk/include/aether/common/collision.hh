#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include "vector.hh"

namespace collision {
    using point = Eigen::Vector3f;
    using inertia = Eigen::Matrix3f;

    struct sphere {

        Eigen::Vector3f pos { 0, 0, 0 };
        float radius = 0.0;

        sphere operator+(const vec3f& rhs) const {
            sphere lhs(*this);
            lhs.pos += Eigen::Vector3f(rhs.x, rhs.y, rhs.z);
            return lhs;
        }

        template<typename SD>
        void serde_visit(SD &sd) {
            sd & pos & radius;
        }
    };

    struct ray {
        Eigen::Vector3f pos, dir;
    };

    struct segment {
        Eigen::Vector3f pos, dir;
        segment operator+(const vec3f& rhs) const {
            segment lhs(*this);
            lhs.pos += Eigen::Vector3f(rhs.x, rhs.y, rhs.z);
            return lhs;
        }
    };

    struct capsule {
        segment s;
        float radius;
        capsule operator+(const vec3f& rhs) const {
            capsule lhs(*this);
            lhs.s.pos += Eigen::Vector3f(rhs.x, rhs.y, rhs.z);
            return lhs;
        }
    };

    static float squared_distance(const segment& s0, const segment& s1) {
        Eigen::Vector3f n1 = s1.dir.cross(s0.dir.cross(s1.dir));
        float t0 = (s1.pos - s0.pos).dot(n1) / s0.dir.dot(n1);
        t0 = std::clamp(t0, 0.0f, 1.0f);
        Eigen::Vector3f p0 = s0.pos + t0 * s0.dir;
        Eigen::Vector3f n0 = s0.dir.cross(s1.dir.cross(s0.dir));
        float t1 = (s0.pos - s1.pos).dot(n0) / s1.dir.dot(n0);
        t1 = std::clamp(t1, 0.0f, 1.0f);
        Eigen::Vector3f p1 = s0.pos + t1 * s0.dir;
        return (p1 - p0).squaredNorm();
    }

    static float squared_distance(const ray& r, const segment& s) {
        Eigen::Vector3f n1 = s.dir.cross(r.dir.cross(s.dir));
        float t0 = (s.pos - r.pos).dot(n1) / r.dir.dot(n1);
        t0 = std::max(t0, 0.0f);
        Eigen::Vector3f p0 = r.pos + t0 * r.dir;
        Eigen::Vector3f n0 = r.dir.cross(s.dir.cross(r.dir));
        float t1 = (r.pos - s.pos).dot(n0) / s.dir.dot(n0);
        t1 = std::clamp(t1, 0.0f, 1.0f);
        Eigen::Vector3f p1 = r.pos + t1 * r.dir;
        return (p1 - p0).squaredNorm();
    }

    static float squared_distance(const point& p, const ray& r) {
        float t = (p - r.pos).dot(r.dir) / r.dir.squaredNorm();
        t = std::max(t, 0.0f);
        return (r.pos + t * r.dir - p).squaredNorm();
    }

    static float squared_distance(const point& p, const segment& s) {
        float t = (p - s.pos).dot(s.dir) / s.dir.squaredNorm();
        t = std::clamp(t, 0.0f, 1.0f);
        return (s.pos + t * s.dir - p).squaredNorm();
    }

    static float squared_distance(const point& p0, const point& p1) {
        return (p1 - p0).squaredNorm();
    }

    static bool intersection(const capsule& c0, const capsule& c1) {
        float r = c0.radius + c1.radius;
        return squared_distance(c0.s, c1.s) < r * r;
    }

    static bool intersection(const ray& r, const sphere& s) {
        return squared_distance(s.pos, r) < s.radius * s.radius;
    }

    static bool intersection(const ray& r, const capsule& c) {
        return squared_distance(r, c.s) < c.radius * c.radius;
    }

    static bool intersection(const point& p, const capsule& c) {
        return squared_distance(p, c.s) < c.radius * c.radius;
    }

    static bool intersection(const sphere& s, const capsule& c) {
        float r = s.radius + c.radius;
        return squared_distance(s.pos, c.s) < r * r;
    }

    static bool intersection(const sphere& s0, const sphere& s1) {
        float r = s0.radius + s1.radius;
        return squared_distance(s0.pos, s1.pos) < r * r;
    }

    static bool intersection(const point& p, const sphere& s) {
        return squared_distance(p, s.pos) < s.radius * s.radius;
    }
}
