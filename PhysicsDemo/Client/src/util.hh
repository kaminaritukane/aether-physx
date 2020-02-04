#pragma once

#include <Eigen/Geometry>
#include <chrono>
#include <aether/common/base_protocol.hh>

struct ui_point {
    vec3f p;
    protocol::base::net_quat quat;
    struct colour c;
    float size;
};

using clock_type = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<clock_type>;
using duration = clock_type::duration;

struct worker_info {
    protocol::base::client_stats stats;
};

enum worker_message_type {
    DEBUG_MSG = 0,
    CLICK_MSG = 1,
};

// http://antongerdelan.net/opengl/raycasting.html
// https://gamedev.stackexchange.com/questions/107902/getting-ray-using-gluunproject-or-inverted-mvp-matrix
static void glhUnProjectf(float winx, float winy, float winwidth, float winheight, Eigen::Matrix4f view, Eigen::Matrix4f projection, float *objectCoordinate) {
    // Homogenous clip coords
    Eigen::Vector4f ray_clip;
    ray_clip[0] = 2.0 * winx/winwidth - 1.0;
    ray_clip[1] = 1.0 - 2.0*winy/winheight;
    ray_clip[2] = -1.0;
    ray_clip[3] = 1.0;
    // Eye coords
    Eigen::Vector4f ray_eye = projection.inverse() * ray_clip;
    ray_eye[2] = -1.0;
    ray_eye[3] = 0.0;
    // World coords
    Eigen::Vector4f ray_wor(view.inverse() * ray_eye);

    objectCoordinate[0]=ray_wor[0];
    objectCoordinate[1]=ray_wor[1];
    objectCoordinate[2]=ray_wor[2];
}

static void unproject_position(
    vec2f &pos,
    const int width, const int height,
    Eigen::Matrix4f view, Eigen::Matrix4f projection,
    Eigen::Vector3f camera_pos)
{
    float coords[3] = { 0.0, 0.0, 0.0 };
    glhUnProjectf(pos.x, pos.y, width, height, view, projection, coords);
    assert(std::isfinite(coords[0]) && std::isfinite(coords[1]) && std::isfinite(coords[2]));
    float amount = -camera_pos.z() / coords[2];
    float ax = camera_pos.x() + amount * coords[0];
    float ay = camera_pos.y() + amount * coords[1];
    pos.x = ax;
    pos.y = ay;
}

static inline Eigen::Matrix4f mat4x4_perspective(
    float y_fov, float aspect, float n, float f)
{
    const float a = 1.f / tan(y_fov / 2.f);
    Eigen::Matrix4f m;
    m <<
        a / aspect, 0.f,  0.f,                        0.f,
        0.f,        a,    0.f,                        0.f,
        0.f,        0.f, -((f + n) / (f - n)),       -1.f,
        0.f,        0.f, -((2.f * f * n) / (f - n)),  0.f;
    return m;
}

static uint64_t pid_to_machine_id(const uint64_t id) {
    return id >> 32;
}
