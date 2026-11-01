#pragma once

#include <raylib.h>

#include <array>
#include <span>
#include <vector>

#include "zioop/gui/DayCycle.hpp"

namespace gui {
    struct PointLight {
        Vector3 position{};
        Vector3 color{};
        float radius{1.0f};
        bool priority{false};
    };

    class Lighting {
    public:
        static constexpr int MaxPointLights = 16;
        static constexpr int ShadowMapResolution = 4096;

        enum class Pass { Shadow, Color };

        Lighting();

        ~Lighting();

        Lighting(const Lighting &) = delete;

        Lighting &operator=(const Lighting &) = delete;

        void setEnvironment(
            const DayCycle::Environment &environment,
            const Camera3D &camera,
            std::vector<PointLight> lights
        );

        void drawSky(const Camera3D &camera, float time) const;

        void beginShadowPass(Vector3 center, float radius);

        void endShadowPass();

        void beginColorPass() const;

        void endColorPass() const;

        void drawModel(
            const Model &model,
            const Matrix &transform,
            Color tint = WHITE,
            std::span<const float> emissive = {},
            float emissiveScale = 1.0f
        ) const;

        void drawMesh(const Mesh &mesh, const Material &material, const Matrix &transform) const;

        void setGlow(Vector3 color) const;

        [[nodiscard]] Pass pass() const {
            return m_pass;
        }

        [[nodiscard]] Shader shader() const {
            return m_lit;
        }

    private:
        static constexpr int ShadowSlot = 11;

        void setEmissive(float value) const;

        Shader m_lit{};
        Shader m_depth{};
        Shader m_sky{};
        RenderTexture2D m_shadowMap{};

        Pass m_pass{Pass::Color};
        DayCycle::Environment m_environment{};
        Matrix m_lightViewProjection{};

        int m_viewPositionLoc{-1};
        int m_keyDirectionLoc{-1};
        int m_keyColorLoc{-1};
        int m_skyAmbientLoc{-1};
        int m_groundAmbientLoc{-1};
        int m_fogColorLoc{-1};
        int m_fogDensityLoc{-1};
        int m_emissiveLoc{-1};
        int m_glowLoc{-1};
        int m_lightViewProjectionLoc{-1};
        int m_shadowMapLoc{-1};
        int m_shadowStrengthLoc{-1};
        int m_pointCountLoc{-1};
        int m_pointPositionLoc{-1};
        int m_pointColorLoc{-1};
        int m_pointRadiusLoc{-1};

        int m_skyResolutionLoc{-1};
        int m_skyForwardLoc{-1};
        int m_skyRightLoc{-1};
        int m_skyUpLoc{-1};
        int m_skyTanHalfFovLoc{-1};
        int m_skySunDirectionLoc{-1};
        int m_skySunColorLoc{-1};
        int m_skyZenithLoc{-1};
        int m_skyHorizonLoc{-1};
        int m_skyNightLoc{-1};
        int m_skyTimeLoc{-1};
    };
}
