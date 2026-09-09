#include "zioop/gui/Agent.hpp"

#include <stdexcept>

namespace gui {

    Agent::Agent(const std::string& asset) {

        m_model = LoadModel(asset.c_str());

        if (!IsModelValid(m_model)) {
            throw std::runtime_error(
                "Unable to load model: " + asset
            );
        }

        m_animations = LoadModelAnimations(
            asset.c_str(),
            &m_animCount
        );
    }


    Agent::~Agent() {

        if (m_animations != nullptr) {
            UnloadModelAnimations(
                m_animations,
                m_animCount
            );
        }

        UnloadModel(m_model);
    }

}