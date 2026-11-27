#pragma once

#include <cmath>

namespace usylibpp::ui {

[[nodiscard]] constexpr float lerp(float a, float b, float t) noexcept {
    return a + ((b - a) * t);
}

/// Exponential smoothing toward `target`; snaps cleanly at `eps` so idle UIs stop repainting.
struct AnimFloat {
    float value = 0.0f;
    float target = 0.0f;

    void snap(float v) noexcept {
        value = v;
        target = v;
    }

    void set(float v, bool animate) noexcept {
        target = v;
        if (!animate) value = v;
    }

    [[nodiscard]] bool active(float eps = 0.002f) const noexcept {
        return std::abs(target - value) >= eps;
    }

    bool step(float dt, float speed = 22.0f, float eps = 0.002f) noexcept {
        const float d = target - value;
        if (std::abs(d) < eps) {
            value = target;
            return false;
        }
        value += d * (1.0f - std::exp(-dt * speed));
        if (std::abs(target - value) < eps) {
            value = target;
            return false;
        }
        return true;
    }
};

} // namespace usylibpp::ui
