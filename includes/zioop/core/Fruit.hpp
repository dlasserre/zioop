#pragma once
#include "Resource.hpp"

namespace core {

    class Fruit : public Resource {
    public:
        using Resource::Resource;

    protected:
        void decaying() override {
            ++m_rotting;
        }
    };

}
