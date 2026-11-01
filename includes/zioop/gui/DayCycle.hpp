#pragma once

#include <raylib.h>

#include <string>

namespace gui {
    class DayCycle {
    public:
        struct Environment {
            Vector3 sunDirection{};
            Vector3 keyDirection{};
            Vector3 keyColor{};
            Vector3 sunColor{};
            Vector3 skyAmbient{};
            Vector3 groundAmbient{};
            Vector3 zenithColor{};
            Vector3 horizonColor{};
            float night{0.0f};
            float shadowStrength{1.0f};
            float fogDensity{0.02f};
        };

        void update(float deltaTime);

        void togglePause();

        void advance(float hours);

        void faster();

        void slower();

        [[nodiscard]] Environment environment() const;

        [[nodiscard]] std::string clock() const;

        [[nodiscard]] float speed() const {
            return m_speed;
        }

        [[nodiscard]] bool paused() const {
            return m_paused;
        }

    private:
        float m_hours{9.0f};
        float m_speed{1.0f};
        bool m_paused{false};
    };
}
