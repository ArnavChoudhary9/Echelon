#pragma once

#include "Layer.h"
#include "Overlay.h"
#include <vector>

namespace Echelon {
    /**
     * @brief Manages a stack of layers and overlays in the application.
     * 
     */
    class LayerStack {
    public:
        LayerStack() = default;
        ~LayerStack();

        /**
         * @brief Pushes a layer onto the stack.
         * 
         * @param layer The layer to push.
         */
        void PushLayer(Layer* layer);

        /**
         * @brief Pushes an overlay onto the stack.
         * 
         * @param overlay The overlay to push.
         */
        void PushOverlay(Layer* overlay);

        /**
         * @brief Pops a layer from the stack.
         * 
         * @param layer The layer to pop.
         */
        void PopLayer(Layer* layer);

        /**
         * @brief Pops an overlay from the stack.
         * 
         * @param overlay The overlay to pop.
         */
        void PopOverlay(Layer* overlay);

        /**
         * @brief Returns an iterator to the beginning of the layer stack.
         * 
         * @return std::vector<Layer*>::iterator 
         */
        std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }

        /**
         * @brief Returns an iterator to the end of the layer stack.
         * 
         * @return std::vector<Layer*>::iterator 
         */
        std::vector<Layer*>::iterator end() { return m_Layers.end(); }

    private:
        std::vector<Layer*> m_Layers;
        unsigned int m_LayerInsertIndex = 0;
    };
}
