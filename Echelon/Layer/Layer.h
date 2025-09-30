#pragma once

namespace Echelon {
    /**
     * @brief Represents a layer in the application. Extend this class to create custom layers.
     * 
     */
    class Layer {
    public:
        virtual ~Layer() = default;

        /**
         * @brief Called when the layer is attached to the application.
         * 
         */
        virtual void OnAttach() = 0;

        /**
         * @brief Called when the layer is detached from the application.
         * 
         */
        virtual void OnDetach() = 0;

        /**
         * @brief Called every frame to update the layer.
         * 
         * @param deltaTime The time elapsed since the last frame.
         */
        virtual void OnUpdate(float) = 0;
    };
}
