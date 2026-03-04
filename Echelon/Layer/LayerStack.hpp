#pragma once

#include "Layer.hpp"
#include "Overlay.hpp"
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
        void PushLayer(Layer*);

        /**
         * @brief Pushes an overlay onto the stack.
         * 
         * @param overlay The overlay to push.
         */
        void PushOverlay(Layer*);

        /**
         * @brief Pops a layer from the stack.
         * 
         * @param layer The layer to pop.
         */
        void PopLayer(Layer*);

        /**
         * @brief Pops an overlay from the stack.
         * 
         * @param overlay The overlay to pop.
         */
        void PopOverlay(Layer*);

        /**
         * @brief Handles an event for all layers in the stack.
         * 
         * @param event The event to handle.
         * @return true If the event was handled by any layer in the stack.
         * @return false If the event was not handled by any layer in the stack.
         */
        bool OnEvent(Event&);

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
