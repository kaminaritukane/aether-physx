#pragma once

#include <Eigen/Geometry>
#include "vector.hh"
#include "net.hh"

inline Eigen::Vector3f vec3f_to_eigen(const vec3f& v) {
    return Eigen::Vector3f{v.x, v.y, v.z};
}

inline vec3f vec3f_from_eigen(const Eigen::Vector3f& v) {
    return vec3f{v.x(), v.y(), v.z()};
}

inline net_quat net_encode_quaternion(const Eigen::Quaternionf& q) {
    return net_quat{q.x(), q.y(), q.z(), q.w()};
}

inline Eigen::Quaternionf net_decode_quaternion(const net_quat& q) {
    return Eigen::Quaternionf{q.w, q.x, q.y, q.z};
}
