#pragma once
#include <vector>
#include <chrono>
#include <limits>
#include <tuple>

namespace aether {

namespace netcode {

struct generic_interest_policy {
    float scheduling_granularity_hz = 60.0;
    float per_worker_metadata_frequency_hz = 5.0;
    bool no_player_simulation = true;

    enum class gradient_type {
        constant,
        linear,
    };

    // Each ring is a tuple of a radius, a delay for that radius and the gradient function
    std::vector<std::tuple<float, std::chrono::milliseconds, gradient_type>> rings;

    generic_interest_policy() {
        rings.emplace_back(std::numeric_limits<float>::infinity(), 0, gradient_type::linear);
    }

    template<typename TimePoint>
    std::optional<TimePoint> evaluate(const TimePoint &last_sent, const float distance) const {
        if (rings.empty()) {
            return last_sent;
        } else {
            float previous_radius = 0.0;
            auto previous_delay = std::get<1>(rings.front());

            for(const auto &[ring_radius, ring_delay, ring_gradient] : rings) {
                assert(previous_radius <= ring_radius && "Ring radiuses are not monotonic");
                if (distance <= ring_radius) {
                    std::optional<std::chrono::milliseconds> maybe_delay;
                    if (ring_gradient == gradient_type::constant) {
                        maybe_delay = { ring_delay };
                    } else if (ring_gradient == gradient_type::linear) {
                        const double distance_into_ring = distance - previous_radius;
                        const double ring_width = ring_radius - previous_radius;
                        // We need to be careful here to avoid a division by zero
                        const double fraction_into_ring =
                            ring_width == 0.0 ? 0.0 : distance_into_ring / ring_width;
                        const auto delay = previous_delay * (1.0 - fraction_into_ring) +
                            ring_delay * fraction_into_ring;
                        maybe_delay = (previous_delay + ring_delay);
                    }
                    assert(maybe_delay.has_value());
                    return last_sent + maybe_delay.value();
                }

                previous_radius = ring_radius;
                previous_delay = ring_delay;
            }
            // The entity is outside the largest ring
            return std::nullopt;
        }
    }

    static generic_interest_policy none() {
        return generic_interest_policy{};
    }

    void set_has_players(const bool has_players) {
        no_player_simulation = !has_players;
    }

    bool has_players() const {
        return !no_player_simulation;
    }

    float get_cut_off() const {
        if (rings.empty()) {
            return 0.0;
        } else {
            return std::get<0>(rings.back());
        }
    }

    static generic_interest_policy default_3d(const bool has_players) {
        generic_interest_policy result;
        auto &rings = result.rings;
        rings.clear();
        rings.emplace_back(50.0, 0, gradient_type::constant);
        rings.emplace_back(200.0, 500, gradient_type::linear);
        result.set_has_players(has_players);
        return result;
    }

    static generic_interest_policy default_2d(const bool has_players) {
        generic_interest_policy result;
        auto &rings = result.rings;
        rings.clear();
        rings.emplace_back(50.0, 0, gradient_type::constant);
        rings.emplace_back(200.0, 500, gradient_type::linear);
        result.set_has_players(has_players);
        return result;
    }
};

}

}
