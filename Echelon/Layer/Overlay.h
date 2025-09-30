#pragma once

#include "Layer.h"

namespace Echelon {
    class Overlay : public Layer {
    public:
        virtual ~Overlay() = default;

        virtual void OnImGUIBegin() = 0;
        virtual void OnImGUIRender() = 0;
        virtual void OnImGUIEnd() = 0;
    };
}