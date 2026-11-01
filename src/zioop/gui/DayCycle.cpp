#include "zioop/gui/DayCycle.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace gui {
    namespace {
        constexpr float HoursPerSecond = 24.0f / 360.0f;
        constexpr float MinSpeed = 0.25f;
        constexpr float MaxSpeed = 64.0f;

        float smoothstep(const float edge0, const float edge1, const float x) {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        Vector3 mix(const Vector3 a, const Vector3 b, const float t) {
            return Vector3Lerp(a, b, t);
        }
    }

    void DayCycle::update(const float deltaTime) {
        if (m_paused) {
            return;
        }
        m_hours = std::fmod(m_hours + deltaTime * HoursPerSecond * m_speed, 24.0f);
    }

    void DayCycle::togglePause() {
        m_paused = !m_paused;
    }

    void DayCycle::advance(const float hours) {
        m_hours = std::fmod(m_hours + hours, 24.0f);
    }

    void DayCycle::faster() {
        m_speed = std::min(m_speed * 2.0f, MaxSpeed);
    }

    void DayCycle::slower() {
        m_speed = std::max(m_speed * 0.5f, MinSpeed);
    }

    std::string DayCycle::clock() const {
        const int minutes = static_cast<int>(m_hours * 60.0f) % (24 * 60);
        return std::format("{:02}:{:02}", minutes / 60, minutes % 60);
    }

    DayCycle::Environment DayCycle::environment() const {
        const float angle = m_hours / 24.0f * 2.0f * PI - PI / 2.0f;
        const Vector3 sun = Vector3Normalize({std::cos(angle), std::sin(angle), -0.45f});
        const float elevation = sun.y;

        const float day = smoothstep(-0.05f, 0.25f, elevation);
        const float night = 1.0f - smoothstep(-0.25f, 0.0f, elevation);
        const float twilight = std::clamp(1.0f - std::abs(elevation) / 0.3f, 0.0f, 1.0f);

        const Vector3 sunTint = mix({1.0f, 0.42f, 0.16f}, {1.0f, 0.93f, 0.82f}, smoothstep(0.0f, 0.45f, elevation));
        const float sunIntensity = 2.6f * smoothstep(-0.02f, 0.18f, elevation);
        const float moonIntensity = 0.35f * smoothstep(-0.02f, 0.2f, -elevation);

        Environment environment;
        environment.sunDirection = sun;
        environment.night = night;
        environment.sunColor = Vector3Scale(sunTint, 1.0f + twilight);

        if (elevation >= 0.0f) {
            environment.keyDirection = sun;
            environment.keyColor = Vector3Scale(sunTint, sunIntensity);
            environment.shadowStrength = smoothstep(0.0f, 0.15f, elevation) * 0.85f;
        } else {
            environment.keyDirection = Vector3Negate(sun);
            environment.keyColor = Vector3Scale({0.45f, 0.55f, 0.9f}, moonIntensity);
            environment.shadowStrength = smoothstep(0.0f, 0.15f, -elevation) * 0.5f;
        }

        Vector3 zenith = mix({0.004f, 0.007f, 0.02f}, {0.10f, 0.30f, 0.75f}, day);
        zenith = mix(zenith, {0.15f, 0.15f, 0.35f}, twilight * 0.6f);
        Vector3 horizon = mix({0.02f, 0.03f, 0.06f}, {0.55f, 0.70f, 0.90f}, day);
        horizon = mix(horizon, {0.95f, 0.45f, 0.20f}, twilight * 0.75f);
        environment.zenithColor = zenith;
        environment.horizonColor = horizon;

        environment.skyAmbient = Vector3Add(
            mix({0.03f, 0.04f, 0.09f}, {0.35f, 0.42f, 0.55f}, day),
            Vector3Scale({0.15f, 0.08f, 0.05f}, twilight)
        );
        environment.groundAmbient = mix({0.015f, 0.015f, 0.02f}, {0.18f, 0.15f, 0.10f}, day);
        environment.fogDensity = 0.018f + 0.012f * night;

        return environment;
    }
}
