#pragma once

#include "Layer.h"

namespace Echelon {
    /**
     * @brief Represents an overlay layer in the application. Overlays are rendered on top of regular layers.
     * 
     */
    class Overlay : public Layer {
    public:
        virtual ~Overlay() = default;

        /**
         * @brief Called when the ImGui frame begins.
         * 
         */
        virtual void OnImGUIBegin() = 0;

        /**
         * @brief Called to render ImGui elements.
         * 
         */
        virtual void OnImGUIRender() = 0;

        /**
         * @brief Called when the ImGui frame ends.
         * 
         */
        virtual void OnImGUIEnd() = 0;
    };
}