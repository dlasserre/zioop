#pragma once

#include <string>

#include <raylib.h>

namespace gui {

    class Agent {
    public:
        explicit Agent(const std::string& asset);

        ~Agent();

        // Pas de copie
        Agent(const Agent&) = delete;
        Agent& operator=(const Agent&) = delete;

        [[nodiscard]]
        Model& getModel() {
            return m_model;
        }

        [[nodiscard]]
        const Model& getModel() const {
            return m_model;
        }

        [[nodiscard]]
        ModelAnimation* getAnimations() const {
            return m_animations;
        }

        [[nodiscard]]
        unsigned int getAnimationCount() const {
            return m_animCount;
        }

    private:
        Model m_model{};

        ModelAnimation* m_animations{nullptr};

        int m_animCount{0};
    };

}
