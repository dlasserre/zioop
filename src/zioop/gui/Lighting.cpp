#include "zioop/gui/Lighting.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace gui {
    namespace {
        std::string shaderPath(const char *name) {
            return (std::filesystem::path(GetApplicationDirectory()) / "assets" / "shaders" / name).string();
        }

        Shader loadShader(const char *vertex, const char *fragment) {
            const auto vertexPath = vertex ? shaderPath(vertex) : std::string{};
            const auto fragmentPath = shaderPath(fragment);
            const Shader shader = LoadShader(vertex ? vertexPath.c_str() : nullptr, fragmentPath.c_str());
            if (!IsShaderValid(shader)) {
                throw std::runtime_error("Unable to load shader " + fragmentPath);
            }
            return shader;
        }

        RenderTexture2D loadShadowMap(const int size) {
            RenderTexture2D target{};
            target.id = rlLoadFramebuffer();
            target.texture.width = size;
            target.texture.height = size;

            if (target.id == 0) {
                throw std::runtime_error("Unable to create shadow map framebuffer");
            }

            rlEnableFramebuffer(target.id);
            target.depth.id = rlLoadTextureDepth(size, size, false);
            target.depth.width = size;
            target.depth.height = size;
            target.depth.format = 19;
            target.depth.mipmaps = 1;
            rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
            const bool complete = rlFramebufferComplete(target.id);
            rlDisableFramebuffer();

            if (!complete) {
                throw std::runtime_error("Shadow map framebuffer is incomplete");
            }
            return target;
        }

        void setVector(const Shader shader, const int location, const Vector3 value) {
            SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC3);
        }

        void setFloat(const Shader shader, const int location, const float value) {
            SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
        }
    }

    Lighting::Lighting()
        : m_lit(loadShader("lit.vs", "lit.fs")),
          m_depth(loadShader("lit.vs", "depth.fs")),
          m_sky(loadShader(nullptr, "sky.fs")),
          m_shadowMap(loadShadowMap(ShadowMapResolution)) {
        m_lit.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(m_lit, "viewPos");
        m_viewPositionLoc = m_lit.locs[SHADER_LOC_VECTOR_VIEW];
        m_keyDirectionLoc = GetShaderLocation(m_lit, "keyDirection");
        m_keyColorLoc = GetShaderLocation(m_lit, "keyColor");
        m_skyAmbientLoc = GetShaderLocation(m_lit, "skyAmbient");
        m_groundAmbientLoc = GetShaderLocation(m_lit, "groundAmbient");
        m_fogColorLoc = GetShaderLocation(m_lit, "fogColor");
        m_fogDensityLoc = GetShaderLocation(m_lit, "fogDensity");
        m_emissiveLoc = GetShaderLocation(m_lit, "emissive");
        m_glowLoc = GetShaderLocation(m_lit, "glow");
        setGlow({0.0f, 0.0f, 0.0f});
        m_lightViewProjectionLoc = GetShaderLocation(m_lit, "lightVP");
        m_shadowMapLoc = GetShaderLocation(m_lit, "shadowMap");
        m_shadowStrengthLoc = GetShaderLocation(m_lit, "shadowStrength");
        m_pointCountLoc = GetShaderLocation(m_lit, "pointCount");
        m_pointPositionLoc = GetShaderLocation(m_lit, "pointPosition");
        m_pointColorLoc = GetShaderLocation(m_lit, "pointColor");
        m_pointRadiusLoc = GetShaderLocation(m_lit, "pointRadius");

        constexpr int resolution = ShadowMapResolution;
        SetShaderValue(m_lit, GetShaderLocation(m_lit, "shadowMapResolution"), &resolution, SHADER_UNIFORM_INT);

        m_skyResolutionLoc = GetShaderLocation(m_sky, "resolution");
        m_skyForwardLoc = GetShaderLocation(m_sky, "cameraForward");
        m_skyRightLoc = GetShaderLocation(m_sky, "cameraRight");
        m_skyUpLoc = GetShaderLocation(m_sky, "cameraUp");
        m_skyTanHalfFovLoc = GetShaderLocation(m_sky, "tanHalfFov");
        m_skySunDirectionLoc = GetShaderLocation(m_sky, "sunDirection");
        m_skySunColorLoc = GetShaderLocation(m_sky, "sunColor");
        m_skyZenithLoc = GetShaderLocation(m_sky, "zenithColor");
        m_skyHorizonLoc = GetShaderLocation(m_sky, "horizonColor");
        m_skyNightLoc = GetShaderLocation(m_sky, "night");
        m_skyTimeLoc = GetShaderLocation(m_sky, "time");
    }

    Lighting::~Lighting() {
        rlUnloadFramebuffer(m_shadowMap.id);
        rlUnloadTexture(m_shadowMap.depth.id);
        UnloadShader(m_sky);
        UnloadShader(m_depth);
        UnloadShader(m_lit);
    }

    void Lighting::setEnvironment(
        const DayCycle::Environment &environment,
        const Camera3D &camera,
        std::vector<PointLight> lights
    ) {
        m_environment = environment;

        setVector(m_lit, m_viewPositionLoc, camera.position);
        setVector(m_lit, m_keyDirectionLoc, environment.keyDirection);
        setVector(m_lit, m_keyColorLoc, environment.keyColor);
        setVector(m_lit, m_skyAmbientLoc, environment.skyAmbient);
        setVector(m_lit, m_groundAmbientLoc, environment.groundAmbient);
        setVector(m_lit, m_fogColorLoc, environment.horizonColor);
        setFloat(m_lit, m_fogDensityLoc, environment.fogDensity);
        setFloat(m_lit, m_shadowStrengthLoc, environment.shadowStrength);

        const Vector3 focus = camera.target;
        std::ranges::sort(lights, {}, [&](const PointLight &light) {
            return Vector3DistanceSqr(light.position, focus) - (light.priority ? 1.0e6f : 0.0f);
        });
        if (lights.size() > MaxPointLights) {
            lights.resize(MaxPointLights);
        }

        std::array<Vector3, MaxPointLights> positions{};
        std::array<Vector3, MaxPointLights> colors{};
        std::array<float, MaxPointLights> radii{};
        for (std::size_t i = 0; i < lights.size(); ++i) {
            positions[i] = lights[i].position;
            colors[i] = lights[i].color;
            radii[i] = lights[i].radius;
        }

        const int count = static_cast<int>(lights.size());
        SetShaderValue(m_lit, m_pointCountLoc, &count, SHADER_UNIFORM_INT);
        SetShaderValueV(m_lit, m_pointPositionLoc, positions.data(), SHADER_UNIFORM_VEC3, MaxPointLights);
        SetShaderValueV(m_lit, m_pointColorLoc, colors.data(), SHADER_UNIFORM_VEC3, MaxPointLights);
        SetShaderValueV(m_lit, m_pointRadiusLoc, radii.data(), SHADER_UNIFORM_FLOAT, MaxPointLights);
    }

    void Lighting::drawSky(const Camera3D &camera, const float time) const {
        const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
        const Vector3 up = Vector3CrossProduct(right, forward);
        const Vector2 resolution{static_cast<float>(GetRenderWidth()), static_cast<float>(GetRenderHeight())};
        const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);

        SetShaderValue(m_sky, m_skyResolutionLoc, &resolution, SHADER_UNIFORM_VEC2);
        setVector(m_sky, m_skyForwardLoc, forward);
        setVector(m_sky, m_skyRightLoc, right);
        setVector(m_sky, m_skyUpLoc, up);
        setFloat(m_sky, m_skyTanHalfFovLoc, tanHalfFov);
        setVector(m_sky, m_skySunDirectionLoc, m_environment.sunDirection);
        setVector(m_sky, m_skySunColorLoc, m_environment.sunColor);
        setVector(m_sky, m_skyZenithLoc, m_environment.zenithColor);
        setVector(m_sky, m_skyHorizonLoc, m_environment.horizonColor);
        setFloat(m_sky, m_skyNightLoc, m_environment.night);
        setFloat(m_sky, m_skyTimeLoc, time);

        BeginShaderMode(m_sky);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
        EndShaderMode();
    }

    void Lighting::beginShadowPass(const Vector3 center, const float radius) {
        m_pass = Pass::Shadow;

        const Vector3 direction = m_environment.keyDirection;
        const Camera3D lightCamera{
            .position = Vector3Add(center, Vector3Scale(direction, 60.0f)),
            .target = center,
            .up = std::abs(direction.y) > 0.99f ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f},
            .fovy = radius * 2.0f,
            .projection = CAMERA_ORTHOGRAPHIC
        };

        BeginTextureMode(m_shadowMap);
        ClearBackground(WHITE);
        rlSetClipPlanes(1.0, 140.0);
        BeginMode3D(lightCamera);
        m_lightViewProjection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    }

    void Lighting::endShadowPass() {
        EndMode3D();
        EndTextureMode();
        rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
        m_pass = Pass::Color;
    }

    void Lighting::beginColorPass() const {
        SetShaderValueMatrix(m_lit, m_lightViewProjectionLoc, m_lightViewProjection);
        constexpr int slot = ShadowSlot;
        SetShaderValue(m_lit, m_shadowMapLoc, &slot, SHADER_UNIFORM_INT);
        rlActiveTextureSlot(ShadowSlot);
        rlEnableTexture(m_shadowMap.depth.id);
        rlActiveTextureSlot(0);
    }

    void Lighting::endColorPass() const {
        rlActiveTextureSlot(ShadowSlot);
        rlDisableTexture();
        rlActiveTextureSlot(0);
    }

    void Lighting::setEmissive(const float value) const {
        if (m_pass == Pass::Color) {
            setFloat(m_lit, m_emissiveLoc, value);
        }
    }

    void Lighting::setGlow(const Vector3 color) const {
        setVector(m_lit, m_glowLoc, color);
    }

    void Lighting::drawMesh(const Mesh &mesh, const Material &material, const Matrix &transform) const {
        Material copy = material;
        copy.shader = m_pass == Pass::Shadow ? m_depth : m_lit;
        setEmissive(0.0f);
        DrawMesh(mesh, copy, transform);
    }

    void Lighting::drawModel(
        const Model &model,
        const Matrix &transform,
        const Color tint,
        const std::span<const float> emissive,
        const float emissiveScale
    ) const {
        const Matrix world = MatrixMultiply(model.transform, transform);
        const Shader shader = m_pass == Pass::Shadow ? m_depth : m_lit;

        for (int i = 0; i < model.meshCount; ++i) {
            const int index = model.meshMaterial[i];
            Material material = model.materials[index];
            material.shader = shader;

            Color &diffuse = material.maps[MATERIAL_MAP_DIFFUSE].color;
            const Color original = diffuse;
            diffuse = {
                static_cast<unsigned char>(original.r * tint.r / 255),
                static_cast<unsigned char>(original.g * tint.g / 255),
                static_cast<unsigned char>(original.b * tint.b / 255),
                static_cast<unsigned char>(original.a * tint.a / 255)
            };

            setEmissive(static_cast<std::size_t>(index) < emissive.size() ? emissive[index] * emissiveScale : 0.0f);
            DrawMesh(model.meshes[i], material, world);
            diffuse = original;
        }
    }
}
