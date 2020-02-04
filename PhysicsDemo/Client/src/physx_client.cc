#include <iostream>
#include <physx_client.hh>

void physx_client::process_packet(void *message_data, size_t count) {
    auto demarshaller = aether::netcode::trivial_marshalling<trivial_marshalling_traits>().create_demarshaller();
    const bool success = demarshaller.decode(message_data, count);
    assert(success && "Failed to decode packet from simulation");

    const auto headers = demarshaller.get_worker_data();
    for(const auto &[id, header] : headers) {
        if (id + 1 > num_workers) {
            cells.resize(id + 1);
            vertices.resize(id + 1);
            for (uint64_t i = num_workers; i < id + 1; i++) {
                cells[i].code = 0;
                cells[i].level = -1;
                cells[i].pid = 0;
            }
            num_workers = id + 1;
        }

        vertices[id].stats = header.stats;
        if (!header.cell_dying) {
            cells[id] = header.cell;
        } else {
            cells[id].code = 0;
            cells[id].level = -1;
            cells[id].pid = 0;
        }
    }

    const auto message_entities = demarshaller.get_entities();
    for(size_t entity_id = 0; entity_id < message_entities.size(); ++entity_id) {
        const auto &entity = message_entities[entity_id];
        const vec3f position = protocol::base::net_decode_position_3f(entity.net_encoded_position);
        ui_point point;
        point.p = { position.x, position.y, position.z };

        /*
        printf("<><><><> id=%d x=%f y=%f z=%f\n\n", entity_id, position.x, position.y, position.z);
        fflush(stdout);
        */

        point.size = entity.size;
        point.c = net_decode_color(entity.net_encoded_color);

        // please note that net_quat contains floats in the following order float x, y, z, w;  (w is the last variable here)
        point.quat = entity.net_encoded_orientation; 

        const auto id = get_entity_id(entity);
        entities[id] = point;
    }
}

void physx_client::authenticate() {
    if(token) {
        repstate.authenticate_player_id_with_token(current_player, *token);
    } else {
        repstate.authenticate_player_id(current_player);
    }
}

void physx_client::error_callback(int error, const char* description) {
    fprintf(stderr, "glfw error: %s\n", description);
}

void physx_client::cursor_pos_callback_wrapper(GLFWwindow* window, const double x, const double y) {
    auto *me = static_cast<physx_client *>(glfwGetWindowUserPointer(window));
    me->cursor_pos_callback(window, x, y);
}

vec2f physx_client::unproject(const vec2f &_position) {
    auto position = _position;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float ratio = width / static_cast<float>(height);

    Eigen::Affine3f view = Eigen::Affine3f::Identity();
    view.translation() = -camera_pos;
    Eigen::Matrix4f projection = mat4x4_perspective(120 * 2 * M_PI / 360, ratio, 0.1, 100);

    unproject_position(position, width, height, view.matrix(), projection.matrix(), camera_pos);
    return position;
}

void physx_client::cursor_pos_callback(GLFWwindow* window, const double x, const double y) {
    Eigen::Vector2f cursor_pos { (float) x, (float) y };
    Eigen::Vector2f delta = cursor_pos - prev_mouse_pos;
    delta.x() *= -1;
    this->prev_mouse_pos = cursor_pos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) {
        camera_pos.x() += 0.01 * SPEED * delta.x();
        camera_pos.y() += 0.01 * SPEED * delta.y();
        return;
    }

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    double mousex, mousey;
    mousex = x / width - 0.5;
    mousey = height - y;
    mousey = mousey / height - 0.5;

    const float mouse_x_sensitivity = 4.0;
    const float mouse_y_sensitivity = 4.0;

    camera_orientation = {
        Eigen::AngleAxisf(mouse_x_sensitivity * mousex, Eigen::Vector3f::UnitY()) *
        Eigen::AngleAxisf(mouse_y_sensitivity * -mousey, Eigen::Vector3f::UnitX())
    };
    camera_orientation = camera_orientation.normalized();
}

void physx_client::mouse_button_callback_wrapper(GLFWwindow * window, int button, int action, int mods) {
    auto *me = static_cast<physx_client *>(glfwGetWindowUserPointer(window));
    me->mouse_button_callback(window, button, action, mods);
}

void physx_client::mouse_button_callback(GLFWwindow * window, int button, int action, int mods) {
}

void physx_client::scroll_callback(GLFWwindow * window, double dx, double dy) {
    auto * me = static_cast<physx_client *>(glfwGetWindowUserPointer(window));
    me->camera_pos.z() += 0.5 * SPEED * dy;
}

void physx_client::key_callback_wrapper(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto * me = static_cast<physx_client *>(glfwGetWindowUserPointer(window));
    me->key_callback(window, key, scancode, action, mods);
}

void physx_client::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    float speed = action == GLFW_PRESS   ?  SPEED
                : action == GLFW_RELEASE ? -SPEED
                : 0;

    auto * me = static_cast<physx_client *>(glfwGetWindowUserPointer(window));

    switch (key) {
    case GLFW_KEY_W:
        me->camera_velocity.z() -= speed;
        break;
    case GLFW_KEY_A:
    case GLFW_KEY_LEFT:
        me->camera_velocity.x() -= speed;
        break;
    case GLFW_KEY_S:
        me->camera_velocity.z() += speed;
        break;
    case GLFW_KEY_D:
    case GLFW_KEY_RIGHT:
        me->camera_velocity.x() += speed;
        break;
    case GLFW_KEY_SPACE:
    case GLFW_KEY_UP:
        me->camera_velocity.y() += speed;
        break;
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_DOWN:
        me->camera_velocity.y() -= speed;
        break;
    case GLFW_KEY_SEMICOLON:
        if (action == GLFW_PRESS)
            glfwSetInputMode(
                window,
                GLFW_CURSOR,
                glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED
                    ? GLFW_CURSOR_NORMAL
                    : GLFW_CURSOR_DISABLED);
        break;
    }

    if (action != GLFW_PRESS) { return; }

    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
        glfwSetWindowShouldClose(window, true);
    } else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        current_player = key - GLFW_KEY_0;
        me->authenticate();
    }
}

physx_client::physx_client(const char * host, const char * port, std::optional<std::array<unsigned char, 32>> token)
    : repstate(host, port), token(std::move(token))
      //repclient_init_record(argv[1], argv[2], nullptr);
      //repclient_init_playback(nullptr);
{
    //initialise glfw and window
    authenticate();
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) exit(EXIT_FAILURE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, 1);
    glfwWindowHint(GLFW_SAMPLES, 4);
    window = glfwCreateWindow(800, 600, "physx demo", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    glewInit();

    char error_log[4096] = {0};
    program_point_vertex = glCreateShaderProgramv(
        GL_VERTEX_SHADER, 1, &point_vertex_shader_text);
    glGetProgramInfoLog(program_point_vertex, sizeof(error_log), nullptr, error_log);
    if (error_log[0] != 0) {
        fprintf(stderr, "%s\n", error_log);
        exit(EXIT_FAILURE);
    }
    GLuint program_point_fragment = glCreateShaderProgramv(
        GL_FRAGMENT_SHADER, 1, &point_fragment_shader_text);
    glGetProgramInfoLog(program_point_fragment, sizeof(error_log),
                        nullptr, error_log);
    if (error_log[0] != 0) {
        fprintf(stderr, "%s\n", error_log);
        exit(EXIT_FAILURE);
    }

    glGenProgramPipelines(1, &pipeline_point);
    glUseProgramStages(
        pipeline_point, GL_VERTEX_SHADER_BIT, program_point_vertex);
    glUseProgramStages(
        pipeline_point, GL_FRAGMENT_SHADER_BIT, program_point_fragment);

    program_line_vertex = glCreateShaderProgramv(
        GL_VERTEX_SHADER, 1, &line_vertex_shader_text);
    glGetProgramInfoLog(program_line_vertex, sizeof(error_log), nullptr,
                        error_log);
    if (error_log[0] != 0) {
        fprintf(stderr, "%s\n", "vertex shader error");
        fprintf(stderr, "%s\n", error_log);
        exit(EXIT_FAILURE);
    }
    GLuint program_line_geometry = glCreateShaderProgramv(
        GL_GEOMETRY_SHADER, 1, &line_geometry_text);
    glGetProgramInfoLog(
        program_line_geometry, sizeof(error_log), nullptr, error_log);
    if (error_log[0] != 0) {
        fprintf(stderr, "%s\n", "Geometry shader error");
        fprintf(stderr, "%s\n", error_log);
        exit(EXIT_FAILURE);
    }
    GLuint program_line_fragment = glCreateShaderProgramv(
        GL_FRAGMENT_SHADER, 1, &line_fragment_shader_text);
    glGetProgramInfoLog(
        program_line_fragment, sizeof(error_log), nullptr, error_log);
    if (error_log[0] != 0) {
        fprintf(stderr, "%s\n", "frag shader error");
        fprintf(stderr, "%s\n", error_log);
        exit(EXIT_FAILURE);
    }
    GLuint program_line2d_fragment = glCreateShaderProgramv(
        GL_FRAGMENT_SHADER, 1, &line2d_fragment_shader_text);
    glGetProgramInfoLog(
        program_line_fragment, sizeof(error_log), nullptr, error_log);
    if (error_log[0] != 0) {
        fprintf(stderr, "%s\n", "2d frag shader error");
        fprintf(stderr, "%s\n", error_log);
        exit(EXIT_FAILURE);
    }

    glGenVertexArrays(1, &vao_point);
    glBindVertexArray(vao_point);

    //setup point vertices
    glGenBuffers(1, &buffer_point_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_point_vertices);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    p_mvp_location = glGetUniformLocation(program_point_vertex, "mvp");
    GLint vpos_location = glGetAttribLocation(program_point_vertex, "vpos");
    GLint vquat_location = glGetAttribLocation(program_point_vertex, "vquat");
    GLint vcol_location = glGetAttribLocation(program_point_vertex, "vcol");
    GLint vsize_location = glGetAttribLocation(program_point_vertex, "vsize");
    GLint mpos_location = glGetAttribLocation(program_point_vertex, "mesh_pos");
    glEnableVertexAttribArray(vpos_location);
    glEnableVertexAttribArray(vquat_location);
    glEnableVertexAttribArray(vcol_location);
    glEnableVertexAttribArray(vsize_location);
    glVertexAttribPointer(vpos_location, 3, GL_FLOAT, GL_FALSE,
                          sizeof(struct ui_point),
                          (void*)offsetof(struct ui_point, p));
    glVertexAttribPointer(vquat_location, 4, GL_FLOAT, GL_FALSE,
                          sizeof(struct ui_point),
                          (void*)offsetof(struct ui_point, quat));
    glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
                          sizeof(struct ui_point),
                          (void*)offsetof(struct ui_point, c));
    glVertexAttribPointer(vsize_location, 1, GL_FLOAT, GL_FALSE,
                          sizeof(struct ui_point),
                          (void*)offsetof(struct ui_point, size));
    glVertexAttribDivisor(vpos_location, 1);
    glVertexAttribDivisor(vquat_location, 1);
    glVertexAttribDivisor(vcol_location, 1);
    glVertexAttribDivisor(vsize_location, 1);
    GLuint buffer_point_mesh;
    glGenBuffers(1, &buffer_point_mesh);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_point_mesh);

    // sphere
    /*
    glBufferData(GL_ARRAY_BUFFER, sizeof(sphere_vertices),
                 sphere_vertices.data(), GL_STATIC_DRAW);
    */
    // cube
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices),
                 cube_vertices.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(mpos_location);
    glVertexAttribPointer(mpos_location, 3, GL_FLOAT, GL_FALSE, sizeof(vec<3, float>), nullptr);
    glVertexAttribDivisor(mpos_location, 0);

    //setup vao line
    glGenVertexArrays(1, &vao_line);
    glBindVertexArray(vao_line);

    glGenBuffers(1, &buffer_point_indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_point_indices);
    // sphere
    /*
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sphere_indices.size() * sizeof(GLuint[3]),
                 sphere_indices.data(),
                 GL_STATIC_DRAW);
    */
    // cube
    
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 cube_indices.size() * sizeof(GLuint[3]),
                 cube_indices.data(),
                 GL_STATIC_DRAW);

    l_mvp_location = glGetUniformLocation(program_line_vertex, "mvp");
    l_vpos_location = glGetAttribLocation(program_line_vertex, "vpos");
    l_pid_location = glGetUniformLocation(program_line_vertex, "pid");

    //setup cube vertices
    cube_renderer.num_vertices = cube_vertices_aether_cell.size();
    cube_renderer.modes = {GL_LINES};
    cube_renderer.stride = sizeof(cube_vertices_aether_cell[0]);
    glGenProgramPipelines(1, &cube_renderer.pipeline);
    glUseProgramStages(cube_renderer.pipeline,
                       GL_VERTEX_SHADER_BIT, program_line_vertex);
    glUseProgramStages(cube_renderer.pipeline,
                       GL_FRAGMENT_SHADER_BIT, program_line_fragment);
    glUseProgramStages(cube_renderer.pipeline, GL_GEOMETRY_SHADER_BIT, program_line_geometry);
    glGenBuffers(1, &cube_renderer.buffer);
    glBindBuffer(GL_ARRAY_BUFFER, cube_renderer.buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 cube_vertices_aether_cell.size() * sizeof(cube_vertices_aether_cell[0]),
                 cube_vertices_aether_cell.data(),
                 GL_STATIC_DRAW);

    //setup line vertices
    square_renderer.num_vertices = square_vertices_aether_cell.size();
    square_renderer.modes = { { GL_TRIANGLE_FAN, GL_LINE_LOOP } };
    square_renderer.stride = sizeof(square_vertices_aether_cell[0]);
    glGenProgramPipelines(1, &square_renderer.pipeline);
    glUseProgramStages(square_renderer.pipeline,
                       GL_VERTEX_SHADER_BIT, program_line_vertex);
    glUseProgramStages(square_renderer.pipeline,
                       GL_FRAGMENT_SHADER_BIT, program_line2d_fragment);
    glGenBuffers(1, &square_renderer.buffer);
    glBindBuffer(GL_ARRAY_BUFFER, square_renderer.buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 square_vertices_aether_cell.size() * sizeof(square_vertices_aether_cell[0]),
                 square_vertices_aether_cell.data(),
                 GL_STATIC_DRAW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_DEPTH_CLAMP);
    glEnable(GL_BLEND);
    glEnable(GL_MULTISAMPLE);
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetMouseButtonCallback(window, mouse_button_callback_wrapper);
    glfwSetCursorPosCallback(window, cursor_pos_callback_wrapper);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback_wrapper);

    glfwSetTime(0);
    glfwSetWindowUserPointer(window, static_cast<void *>(this));
}

void physx_client::update_camera() {
    double dt = glfwGetTime() - previous_time;
    previous_time += dt;
    camera_pos += static_cast<float>(dt) * camera_velocity;

    glfwGetWindowSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);

    view.translation() = -camera_pos;
    view.linear() = camera_orientation.toRotationMatrix();
    projection = mat4x4_perspective(
        60 * 2 * M_PI / 360,
        static_cast<float>(window_width) / window_height,
        1,
        1000);
}

void physx_client::print_statistics() {
    const statistic stat = stats.get_sample_per_second(1.0);
    printf("Data in: %f KB/s\n", stat.bytes / 1024.0);
    protocol::base::client_stats client_stats_accum = { 0 };
    for(size_t i = 0; i < vertices.size(); ++i) {
        if (cells[i].level != static_cast<uint64_t>(-1)) {
            printf("Worker %zu: num_agents=%" PRIu64 ", num_ghost=%" PRIu64 "\n",
                   i,
                   vertices[i].stats.num_agents,
                   vertices[i].stats.num_agents_ghost);
            client_stats_accum.num_agents += vertices[i].stats.num_agents;
            client_stats_accum.num_agents_ghost +=
                vertices[i].stats.num_agents_ghost;
        }
    }
    printf("Total: num_agents=%" PRIu64 ", num_ghost=%" PRIu64 "\n\n",
           client_stats_accum.num_agents,
           client_stats_accum.num_agents_ghost);
    fflush(stdout);
}

physx_client::~physx_client() {
}

bool physx_client::tick() {
    if (current_frame % 20 == 0) {
        print_statistics();
    }

    //handle user input
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) {
        glfwDestroyWindow(window);
		glfwTerminate();
        return false;
    }

    update_camera();

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    size_t msg_size;
    for (void * msg; (msg = repstate.tick(&msg_size)); ) {
		process_packet(msg, msg_size);
        statistic stat;
        stat.bytes = msg_size;
        stats += stat;
    }

    const bool debug_interaction = false;
    if (debug_interaction && current_frame % 100 == 0) {
        char str[256];
        int n = snprintf(str, 256,
                         "Interaction test in frame %zu from OpenGL client",
                         current_frame);
        assert(n < 256);

        std::vector<unsigned char> buf(1+1+strlen(str));
        buf[0] = DEBUG_MSG;
        buf[1] = strlen(str);
        memcpy(&buf[1+1], str, strlen(str));
        repstate.send(&buf[0], buf.size());
    }

    std::vector<ui_point> entity_vertices;
    entity_vertices.reserve(entities.size());
    for(const auto &[_, entity] : entities) {
        entity_vertices.push_back(entity);
    }

    //setup mvp matrix for entities
    Eigen::Matrix4f mvp = projection * view.matrix();

    //render entities
    glBindVertexArray(vao_point);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_point_vertices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_point_indices);
    const size_t num_points = entity_vertices.size();
    glBufferData(GL_ARRAY_BUFFER, sizeof(struct ui_point) * num_points,
                 entity_vertices.data(), GL_DYNAMIC_DRAW);
    glBindProgramPipeline(pipeline_point);
    glProgramUniformMatrix4fv(program_point_vertex, p_mvp_location, 1,
                              GL_FALSE, reinterpret_cast<const GLfloat*>(mvp.data()));
    // sphere
    /*
    glDrawElementsInstanced(
        GL_TRIANGLES, sphere_indices.size() * 3, GL_UNSIGNED_INT,
        nullptr, num_points);
    */
    // cube
    glDrawElementsInstanced(
        GL_TRIANGLES, cube_indices.size() * 3, GL_UNSIGNED_INT,
        nullptr, num_points);

    std::vector<std::reference_wrapper<const cell_renderer>> renderers;
    for (uint64_t i = 0; i < num_workers; i++) {
        if (cells[i].level != static_cast<uint64_t>(-1)) {
            //setup model matrix for lines
            Eigen::Affine3f model = Eigen::Affine3f::Identity();
            model.linear() = Eigen::Matrix3f::Identity() * (1 << cells[i].level);
            const size_t morton_dimension = cells[i].dimension;
            renderers.clear();
            if (morton_dimension == 2) {
                const auto m_vec = morton_2_decode(cells[i].code);
                model.translation() = Eigen::Vector3f(m_vec.x, m_vec.y, 0);
                renderers.push_back(square_renderer);
            } else if (morton_dimension == 3) {
                const auto m_vec = morton_3_decode(cells[i].code);
                model.translation() = Eigen::Vector3f(m_vec.x, m_vec.y, m_vec.z);
                renderers.push_back(cube_renderer);
            } else {
                assert(false && "Unsupported Morton dimension");
            }
            Eigen::Matrix4f mvp = projection * view.matrix() * model.matrix();

            for (const cell_renderer &renderer : renderers) {
                //render cells
                glBindVertexArray(vao_line);
                glBindBuffer(GL_ARRAY_BUFFER, renderer.buffer);
                glBindProgramPipeline(renderer.pipeline);
                glProgramUniformMatrix4fv(
                    program_line_vertex, l_mvp_location, 1, GL_FALSE,
                    reinterpret_cast<const GLfloat*>(mvp.data()));
                glProgramUniform1ui(program_line_vertex, l_pid_location, static_cast<uint32_t>(pid_to_machine_id(cells[i].pid)));
                glEnableVertexAttribArray(l_vpos_location);
                glVertexAttribPointer(l_vpos_location, 3, GL_FLOAT, GL_FALSE,
                    renderer.stride, (void*)offsetof(vec3f, x));
                for (const auto mode : renderer.modes) {
                    glDrawArrays(mode, 0, renderer.num_vertices);
                }
            }
        }
    }

    glfwSwapBuffers(window);
    ++current_frame;

    return true;
}

int main(int argc, char **argv) {
 /*   if (argc < 3 || argc > 4) {
        std::cerr << "usage: " << argv[0] << " hostname port [token]" << std::endl;
        exit(EXIT_FAILURE);
    }*/

	std::string host = "aether-sdk.mshome.net";
	std::string port = "8881";
	std::string token_str = "00000000000000000000000000000001";

    std::optional<std::array<unsigned char, 32>> token{};

	token = std::make_optional<std::array<unsigned char, 32>>();
	std::copy(token_str.c_str(), token_str.c_str() + 32, token->begin());

    physx_client client(host.c_str(), port.c_str(), token);
    while (client.tick());
    return EXIT_SUCCESS;
}
