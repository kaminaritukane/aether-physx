#pragma once

#include <aether/common/serde.hh>
#include <Eigen/Geometry>

template<typename SD>
void serde_visit(SD &sd, Eigen::Quaternionf &q) {
    sd & q.x() & q.y() & q.z() & q.w();
}

template<typename SD>
void serde_visit(SD &sd, Eigen::Vector3f &q) {
    sd & q.x() & q.y() & q.z();
}

template<typename SD>
void serde_visit(SD &sd, Eigen::Matrix3f &q) {
    sd.visit_bytes(q.data(), q.size() * sizeof(float));
}
