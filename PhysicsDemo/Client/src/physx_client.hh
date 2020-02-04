#pragma once

#define GL_GLEXT_PROTOTYPES
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <Eigen/Geometry>

#include <vector>
#include <optional>
#include <unordered_map>

#include <aether/repclient.hh>
#include <aether/common/statistics.hh>
#include <aether/common/vector.hh>
#include <aether/common/colour.hh>
#include <aether/common/morton/encoding.hh>

#include <aether/common/logging.hh>

#include <aether/generic-netcode/trivial_marshalling.hh>
#include <aether/common/net.hh>
#include <aether/common/eigen_converters.hh>

#include "statistic.hh"
#include "util.hh"
#include "meshes.hh"
#include "shaders.hh"

struct trivial_marshalling_traits {
    using per_worker_data_type = protocol::base::client_message;
    using entity_type = protocol::base::net_point_3d;
    using static_data_type = std::monostate;
};

namespace {

struct cell_renderer {
    GLuint buffer;
    size_t num_vertices;
    std::vector<GLenum> modes;
    GLsizei stride;
    GLuint pipeline;
};

}

struct physx_client {
    static constexpr float SPEED = 50.0;

    static void error_callback(int error, const char* description);
    static void scroll_callback(GLFWwindow * window, double dx, double dy);

    static void cursor_pos_callback_wrapper(GLFWwindow* window, const double x, const double y);
    static void mouse_button_callback_wrapper(GLFWwindow * window, int button, int action, int mods);
    static void key_callback_wrapper(GLFWwindow* window, int key, int scancode, int action, int mods);

    void cursor_pos_callback(GLFWwindow* window, const double x, const double y);
    void mouse_button_callback(GLFWwindow * window, int button, int action, int mods);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

    physx_client(const char * host, const char * port, std::optional<std::array<unsigned char, 32>> token);
    bool tick();
    void update_camera();
    void print_statistics();
    vec2f unproject(const vec2f &position);
    void process_packet(void *message_data, size_t count);
    void authenticate();
    ~physx_client();

    int window_width, window_height;

    uint32_t current_player = 0;
    std::vector<worker_info> vertices;
    uint64_t num_workers = 0;
    std::vector<protocol::base::net_tree_cell> cells;
    std::unordered_map<uint64_t, ui_point> entities;

    repclient repstate;
    GLint p_mvp_location, l_mvp_location;
    GLint l_vpos_location, l_pid_location;
    double previous_time { 0.0 };
    Eigen::Vector2f prev_mouse_pos { 0, 0 };
    Eigen::Vector3f camera_pos { 0, 0, 16 };
    Eigen::Vector3f camera_velocity { 0, 0, 0 };
    Eigen::Quaternionf camera_orientation { Eigen::Quaternionf::Identity() };
    Eigen::Affine3f view { Eigen::Affine3f::Identity() };
    Eigen::Matrix4f projection { Eigen::Matrix4f::Identity() };
    size_t current_frame { 0 };
    statistics<statistic> stats { 60.0 };
    GLFWwindow * window;
    GLuint vao_line;
    cell_renderer square_renderer;
    cell_renderer cube_renderer;
    GLuint program_line_vertex;
    GLuint vao_point;
    GLuint buffer_point_vertices;
    GLuint buffer_point_indices;
    GLuint pipeline_point = 0;
    GLuint program_point_vertex;

    std::optional<std::array<unsigned char, 32>> token;

};
