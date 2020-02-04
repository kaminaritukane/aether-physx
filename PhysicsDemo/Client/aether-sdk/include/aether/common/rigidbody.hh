#pragma once

#include "collision.hh"
#include <cassert>
#include <aether/common/serde.hh>

namespace rigidbody
{
    using Vec3 = Eigen::Vector3f;
    using Quat = Eigen::Quaternionf;
    using Mat33 = Eigen::Matrix3f;

    // Configurations
    static constexpr float MAX_TIME_STEP = 0.04f;   // 400 msec
    static constexpr bool APPLY_WORLD_LIMIT = false;
    static constexpr float WORLD_LIMIT = 2.0e3f;   // recommand 2 Km
    static constexpr float Epsilon = 1.0e-6f;   // 1 um
    static constexpr float DAMP_FRICTION_RATIO = 0.65f;
    static constexpr float INVERSE_INERTIA_MULTIPLIER = 15.0f; // Higher means more rotations

    Mat33 calculate_inverse_inertia(const collision::sphere& s, float inverse_mass);

    class physics_state final {

    public:
        float inverse_mass;
        Vec3 position;
        Quat rotation;

        Vec3 linear_velocity;
        Vec3 angular_velocity;

        float friction;
        float rotation_damping;
        float restitution;
        float max_linear_velocity;

        bool disable_linear_velocity;
        bool disable_angular_velocity;
        bool disable_collision;
        bool disable_response_collision;

    private:
        Vec3 local_center_of_mass;
        Mat33 inverse_inertia;
        Vec3 force;
        Vec3 torque;

    public:
        physics_state()
            : inverse_mass(1.0f)
            , position({0, 0, 0})
            , rotation({1, 0, 0, 0})
            , linear_velocity({0, 0, 0})
            , angular_velocity({0, 0, 0})
            , friction(0.5f)
            , rotation_damping(0.1f)
            , restitution(0.3f)
            , max_linear_velocity(10.0f)
            , disable_linear_velocity(false)
            , disable_angular_velocity(false)
            , disable_collision(false)
            , disable_response_collision(false)
            , local_center_of_mass({0.0f, 0.0f, 0.0f})
            , inverse_inertia(Mat33::Identity())
            , force({0.0f, 0.0f, 0.0f})
            , torque({0.0f, 0.0f, 0.0f})
        {
        }

        physics_state(float inverse_mass
            , const Vec3& position
            , const Quat& rotation
            , const Vec3& linear_velocity
            , const Vec3& angular_velocity
            , float friction
            , float rotation_damping
            , float restitution)
            : inverse_mass(inverse_mass)
            , position(position)
            , rotation(rotation)
            , linear_velocity(linear_velocity)
            , angular_velocity(angular_velocity)
            , friction(friction)
            , rotation_damping(rotation_damping)
            , restitution(restitution)
            , max_linear_velocity(10.0f)
            , disable_linear_velocity(false)
            , disable_angular_velocity(false)
            , disable_collision(false)
            , disable_response_collision(false)
            , local_center_of_mass({0.0f, 0.0f, 0.0f})
            , inverse_inertia(Mat33::Identity())
            , force({0.0f, 0.0f, 0.0f})
            , torque({0.0f, 0.0f, 0.0f})
        {
        }

        bool is_static() const {
            return inverse_mass <= 0.0f;
        }

        float get_mass() const {
            if (is_static())
                return std::numeric_limits<float>::max();

            return 1.0f / inverse_mass;
        }

        Vec3 get_force() const {
            return force;
        }

        Vec3 get_torque() const {
            return torque;
        }

        Vec3 get_center_of_mass() const {
            return position + (rotation * local_center_of_mass);
        }

        Mat33 get_inverse_inertia() const {
            auto rot_mat = rotation.matrix();
            return rot_mat * inverse_inertia * rot_mat.inverse();
        }

        void set_shape(const collision::sphere& shape) {
            local_center_of_mass = shape.pos;
            inverse_inertia = calculate_inverse_inertia(shape, inverse_mass);
        }

        void set_force(const Vec3& force) {

            if (is_static())
                return;

            physics_state::force = force;
        }

        void set_acceleration(const Vec3& acceleration) {
            set_force(get_mass() * acceleration);
        }

        void add_force(const Vec3& force) {

            if (is_static())
                return;

            physics_state::force += force;
        }

        void add_acceleration(const Vec3& acceleration) {
            add_force(get_mass() * acceleration);
        }

        void add_torque(const Vec3& torque) {

            if (is_static())
                return;

            physics_state::torque += torque;
        }

        void add_force_at_position(const Vec3& force, const Vec3& point) {

            if (is_static())
                return;

            physics_state::force += force;

            const auto com = get_center_of_mass();
            auto r = point - com;
            auto t = r.cross(force);

            torque += t;
        }

        void step(float delta_time);

        template<typename SD>
        void serde_visit(SD &sd) {
            sd &
            inverse_mass &
            position &
            rotation &

            linear_velocity &
            angular_velocity &

            friction &
            rotation_damping &
            restitution &
            max_linear_velocity &

            disable_linear_velocity &
            disable_angular_velocity &
            disable_collision &
            disable_response_collision &

            local_center_of_mass &
            inverse_inertia &
            force &
            torque;
        }
    };

    class contact final {

    public:
        physics_state& obj0;
        physics_state& obj1;

        const Vec3 point;
        const Vec3 normal;
        const float penetration;
        const float restitution;

    public:
        contact(physics_state& obj0, physics_state& obj1)
            : obj0(obj0)
            , obj1(obj1)
            , point({0, 0, 0})
            , normal({0, 0, 1})
            , penetration(0.0f)
            , restitution(1.0f)
        {
        }

        contact(physics_state& obj0, physics_state& obj1, const Vec3& point, const Vec3& normal, float penetration, float restitution)
            : obj0(obj0)
            , obj1(obj1)
            , point(point)
            , normal(normal)
            , penetration(penetration)
            , restitution(restitution)
        {
        }

        bool detect(const collision::sphere& sphere0, const collision::sphere& sphere1);
        void solve(float time_step);

    private:
        void restore_penetration(float time_step);
    };

} // namespace RigidBody
