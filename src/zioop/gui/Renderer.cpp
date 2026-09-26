#include "zioop/gui/Renderer.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <set>

namespace gui {
    using Clock = std::chrono::steady_clock;

    static void centerCamera(Camera3D &camera, const gui::MatrixConnection::MatrixSnapshot &matrix) {
        const float span = static_cast<float>(std::max(matrix.width, matrix.height)) * zioop::CELL_SIZE;
        camera.target = {
            static_cast<float>(matrix.width - 1) * zioop::CELL_SIZE / 2.0f, 0.0f,
            static_cast<float>(matrix.height - 1) * zioop::CELL_SIZE / 2.0f
        };
        const float distance = std::max(span, 4.0f);
        camera.position = {
            camera.target.x + distance * 0.85f, distance,
            camera.target.z + distance * 0.85f
        };
    }

    static void GroundMesh(Renderer::ModelRender &mesh) {
        const bool modelReady = IsModelValid(mesh.model);

        if (mesh.modelReady) {
            return;
        }

        const auto bounds = GetModelBoundingBox(mesh.model);
        const float size = std::max({
            bounds.max.x - bounds.min.x,
            bounds.max.y - bounds.min.y,
            bounds.max.z - bounds.min.z
        });
        if (size > 0.0f) {
            mesh.scale = 0.45f / size;
        }
        mesh.baseY = -bounds.min.y * mesh.scale;
        mesh.modelReady = modelReady;
    }

    static int FindAnimation(
        const Renderer::ModelRender &mesh,
        const std::string_view name
    ) {
        if (mesh.animationCount == 0) {
            return -1;
        }

        for (int i = 0; i < mesh.animationCount; ++i) {
            const ModelAnimation &animation = mesh.animations[i];

            if (
                std::string_view(animation.name) == name &&
                animation.frameCount > 0 &&
                IsModelAnimationValid(mesh.model, animation)
            ) {
                return i;
            }
        }

        std::cerr << "Animation '" << name << "' not found or invalid\n";
        return -1;
    }

    static void LoadModel(
        Renderer::ModelRender &mesh,
        const std::string &name
    ) {
        const auto asset = (
            std::filesystem::path(GetApplicationDirectory())
            / "assets"
            / std::format("{}", name)
        ).string();

        if (!FileExists(asset.c_str())) {
            throw std::runtime_error("Unable to load GLTF file.");
        }

        mesh.model = ::LoadModel(asset.c_str());
        if (!IsModelValid(mesh.model)) {
            throw std::runtime_error("Unable to load GLTF model.");
        }

        mesh.animations =
                ::LoadModelAnimations(
                    asset.c_str(),
                    &mesh.animationCount
                );

        mesh.walkAnimation = FindAnimation(mesh, "Walk");
        mesh.idleAnimation = FindAnimation(mesh, "Idle");
        mesh.attackAnimation = FindAnimation(mesh, "Bite_Front");
        mesh.takeAnimation = FindAnimation(mesh, "Bite_InPlace");
        mesh.ritualAnimation = FindAnimation(mesh, "Dance");
        mesh.levelUpAnimation = FindAnimation(mesh, "Jump");
        mesh.deathAnimation = FindAnimation(mesh, "Death");

        GroundMesh(mesh);
    }

    static int ActionAnimation(
        const Renderer::ModelRender &mesh,
        const std::string &action
    ) {
        if (action == "ATTACK") {
            return mesh.attackAnimation;
        }
        if (action == "TAKE") {
            return mesh.takeAnimation;
        }
        return -1;
    }

    // Joue une animation une seule fois ; la relance si elle est déjà en cours.
    static void PlayOnce(Renderer::AgentMotion &motion, const int animation) {
        if (animation < 0) {
            return;
        }
        motion.actionAnimation = animation;
        if (motion.animation == animation) {
            motion.frame = 0;
            motion.animationTimer = 0.0f;
        }
    }

    Renderer::Renderer(
        const int width,
        const int height,
        std::string title
    ) : m_width(width),
        m_height(height),
        m_title(std::move(title)) {
        std::cout << "Creating renderer width=" << width << " height=" << height << std::endl;
        InitWindow(m_width, m_height, m_title.c_str());
        SetTargetFPS(60);

        ModelRender &agentModelRender = m_agentModel;
        ModelRender &resourceModelRender = m_resourceModel;

        const std::string path =
                std::string(GetApplicationDirectory()) + "assets/sol.jpg";

        m_groundTexture = LoadTexture(path.c_str());

        m_groundModel = LoadModelFromMesh(
            GenMeshCube(zioop::CELL_SIZE, 0.05f, zioop::CELL_SIZE)
        );

        m_groundModel.materials[0]
                .maps[MATERIAL_MAP_DIFFUSE]
                .texture = m_groundTexture;

        LoadModel(agentModelRender, "Alien.gltf");

        LoadModel(resourceModelRender, "fruit.gltf");

        DisableCursor();
    }

    void Renderer::DrawAgents() {
        auto &matrix = m_matrix;
        auto &model = m_agentModel;

        for (const auto &[id, agent]: matrix.agents) {
            const auto it = m_agentMotions.find(id);

            const AgentMotion *motion =
                    it != m_agentMotions.end()
                        ? &it->second
                        : nullptr;

            const Vector3 visualPosition =
                    motion ? motion->current : agent.position;

            const Color color = ColorFromHSV(
                static_cast<float>((id % 12) * 30),
                0.7f,
                0.8f
            );

            DrawCylinder(
                visualPosition,
                0.19f, 0.19f, 0.015f,
                16, color
            );

            auto position = visualPosition;

            if (model.modelReady) {
                position.y += model.baseY;

                const float rotation = motion ? motion->rotation : 0.0f;

                if (motion && motion->animation >= 0) {
                    UpdateModelAnimation(
                        model.model,
                        model.animations[motion->animation],
                        motion->frame
                    );
                }

                DrawModelEx(
                    model.model,
                    position,
                    Vector3{0.0f, 1.0f, 0.0f},
                    rotation,
                    Vector3{model.scale, model.scale, model.scale},
                    WHITE
                );
            } else {
                position.y += 0.2f;
                DrawCube(position, 0.25f, 0.4f, 0.25f, color);
            }
        }

        // Cadavres : pose finale de Death, assombris et sans disque de couleur.
        if (model.modelReady && model.deathAnimation >= 0) {
            for (const auto &corpse: m_corpses) {
                UpdateModelAnimation(
                    model.model,
                    model.animations[model.deathAnimation],
                    corpse.frame
                );

                auto position = corpse.position;
                position.y += model.baseY;

                DrawModelEx(
                    model.model,
                    position,
                    Vector3{0.0f, 1.0f, 0.0f},
                    corpse.rotation,
                    Vector3{model.scale, model.scale, model.scale},
                    GRAY
                );
            }
        }
    }

    void Renderer::DrawResources() const {
        auto &matrix = m_matrix;
        auto &model = m_resourceModel;

        for (const auto &resource: matrix.resources) {
            if (IsModelValid(model.model)) {
                auto position = resource.position;
                position.y -= .095f;
                DrawModel(
                    model.model,
                    position,
                    0.3f, // Échelle à adapter à ton asset.
                    WHITE
                );
            }
        }
    }

    void Renderer::updateAgentMotions(
        ModelRender &model,
        const float deltaTime
    ) {
        constexpr float duration = 0.70f;
        constexpr float frameDuration = 1.0f / 60.0f;

        for (auto &motion: m_agentMotions | std::views::values) {
            motion.elapsed += deltaTime;

            const float t = std::clamp(
                motion.elapsed / duration,
                0.0f,
                1.0f
            );

            motion.current = {
                motion.from.x + (motion.target.x - motion.from.x) * t,
                motion.from.y + (motion.target.y - motion.from.y) * t,
                motion.from.z + (motion.target.z - motion.from.z) * t
            };

            const bool moving = motion.elapsed < duration;
            // Priorité : marche, animation ponctuelle, rituel, sinon idle.
            int animation = model.idleAnimation;
            if (moving) {
                animation = model.walkAnimation;
            } else if (motion.actionAnimation >= 0) {
                animation = motion.actionAnimation;
            } else if (motion.ritual && model.ritualAnimation >= 0) {
                animation = model.ritualAnimation;
            }

            if (animation != motion.animation) {
                motion.animation = animation;
                motion.frame = 0;
                motion.animationTimer = 0.0f;
            }

            if (animation < 0) {
                continue;
            }

            const int frameCount = model.animations[animation].frameCount;
            const bool playOnce = !moving && animation == motion.actionAnimation;

            motion.animationTimer += deltaTime;

            while (motion.animationTimer >= frameDuration) {
                motion.animationTimer -= frameDuration;
                if (playOnce && motion.frame + 1 >= frameCount) {
                    motion.actionAnimation = -1; // Action terminée : retour à idle.
                    break;
                }
                motion.frame = (motion.frame + 1) % frameCount;
            }
        }

        // Les cadavres jouent Death une fois, puis restent sur la dernière image.
        if (model.deathAnimation >= 0) {
            const int lastFrame = model.animations[model.deathAnimation].frameCount - 1;
            for (auto &corpse: m_corpses) {
                if (corpse.frame >= lastFrame) {
                    continue;
                }
                corpse.animationTimer += deltaTime;
                while (corpse.frame < lastFrame && corpse.animationTimer >= frameDuration) {
                    corpse.animationTimer -= frameDuration;
                    ++corpse.frame;
                }
            }
        }
    }

    void Renderer::updateMatrix(
        bool &hasSnapshot,
        Clock::time_point &nextRequest
    ) {
        const auto now = Clock::now();
        auto &matrixRequest = m_matrixRequest;
        auto &matrix = m_matrix;
        // auto &MatrixConnection = m_connection;

        if (matrixRequest.valid() &&
            matrixRequest.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                auto snapshot = matrixRequest.get();

                const bool resized =
                        !hasSnapshot ||
                        matrix.width != snapshot.width ||
                        matrix.height != snapshot.height;

                // Cases où un agent vient d'attaquer : ceux qui y disparaissent sont morts.
                std::set<std::pair<std::size_t, std::size_t>> attackCells;

                for (const auto &[id, agent]: snapshot.agents) {
                    auto [it, inserted] = m_agentMotions.try_emplace(id);
                    auto &motion = it->second;

                    // Spawn inconue sur la matrice.
                    if (inserted) {
                        motion.from = agent.position;
                        motion.target = agent.position;
                        motion.current = agent.position;
                        // Ne pas rejouer une action faite avant l'arrivée de la GUI.
                        motion.actionCount = agent.actionCount;
                    } else {
                        const auto &target = agent.position;
                        const bool move =
                                target.x != motion.target.x ||
                                target.y != motion.target.y ||
                                target.z != motion.target.z;
                        if (move) {
                            const float dx = target.x - motion.current.x;
                            const float dz = target.z - motion.current.z;
                            if (dx != 0.0f || dz != 0.0f) {
                                motion.rotation = std::atan2(dx, dz) * RAD2DEG;
                            }

                            motion.from = motion.current;
                            motion.target = target;
                            motion.elapsed = 0.0f;
                            motion.frame = 0;
                            motion.animationTimer = 0.0f;
                        }

                        if (agent.actionCount != motion.actionCount) {
                            motion.actionCount = agent.actionCount;
                            if (agent.action == "ATTACK") {
                                attackCells.insert({agent.x, agent.y});
                            }
                            PlayOnce(motion, ActionAnimation(m_agentModel, agent.action));
                        }

                        // Niveau connu qui augmente : fin du rituel.
                        if (motion.level > 0 && agent.level > motion.level) {
                            PlayOnce(motion, m_agentModel.levelUpAnimation);
                        }
                    }
                    motion.level = agent.level;
                    motion.ritual = agent.ritual;
                }

                constexpr std::size_t MaxCorpses = 64;
                for (auto it = m_agentMotions.begin(); it != m_agentMotions.end();) {
                    if (snapshot.agents.contains(it->first)) {
                        ++it;
                        continue;
                    }
                    // Disparu pendant une attaque sur sa case : il est mort, on garde le cadavre.
                    // Sinon c'est une déconnexion : il disparaît simplement.
                    const auto previous = matrix.agents.find(it->first);
                    if (previous != matrix.agents.end() &&
                        attackCells.contains({previous->second.x, previous->second.y})) {
                        m_corpses.push_back({
                            .position = it->second.current,
                            .rotation = it->second.rotation
                        });
                        if (m_corpses.size() > MaxCorpses) {
                            m_corpses.erase(m_corpses.begin());
                        }
                    }
                    it = m_agentMotions.erase(it);
                }

                matrix = std::move(snapshot);

                if (resized) {
                    m_corpses.clear(); // Nouvelle partie.
                    centerCamera(m_camera, matrix);
                }
                hasSnapshot = true;
            } catch (const std::exception &error) {
                std::cerr << error.what() << '\n';
                nextRequest = Clock::now() + std::chrono::seconds(2);
            }
        }

        if (!matrixRequest.valid() && now >= nextRequest) {
            matrixRequest = std::async(std::launch::async, [this] {
                return m_connection.request();
            });

            nextRequest = now + zioop::REFRESH_INTERVAL;
        }
    }

    void Renderer::DrawGround() {
        if (m_matrix.width == 0 || m_matrix.height == 0) {
            return;
        }

        const float cellsize = zioop::CELL_SIZE;

        // Un cube texturé par case.
        for (std::size_t x = 0; x < m_matrix.width; ++x) {
            for (std::size_t z = 0; z < m_matrix.height; ++z) {
                const Vector3 position{
                    static_cast<float>(x) * cellsize,
                    0.0,
                    static_cast<float>(z) * cellsize
                };

                DrawModel(m_groundModel, position, 1.0f, WHITE);
            }
        }
    }

    void Renderer::drawWorld() {
        bool hasSnapshot = false;
        auto nextRequest = Clock::now();
        auto &model = m_agentModel;

        while (!WindowShouldClose()) {
            const auto now = Clock::now();
            auto &matrix = m_matrix;
            updateMatrix(hasSnapshot, nextRequest);

            updateAgentMotions(model, GetFrameTime());

            if (hasSnapshot && IsKeyPressed(KEY_R)) {
                centerCamera(m_camera, matrix);
            }

            UpdateCamera(&m_camera, CAMERA_FREE);

            BeginDrawing();
            ClearBackground(SKYBLUE);
            BeginMode3D(m_camera);

            DrawGround();
            DrawAgents();
            DrawResources();

            EndMode3D();
            EndDrawing();
        }
    }

    Renderer::~Renderer() {
        EnableCursor();
        if (m_agentModel.animations) {
            UnloadModelAnimations(m_agentModel.animations, m_agentModel.animationCount);
        }
        if (m_agentModel.model.meshes || m_agentModel.model.materials) {
            UnloadModel(m_agentModel.model);
        }

        UnloadModel(m_resourceModel.model);
        UnloadModel(m_groundModel);
        UnloadTexture(m_groundTexture);

        CloseWindow();
    }
}
