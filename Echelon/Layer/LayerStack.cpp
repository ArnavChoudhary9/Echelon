#include "LayerStack.hpp"

#include <algorithm>

namespace Echelon {
    LayerStack::~LayerStack() {
        for (auto& layer : m_Layers) {
            layer->OnDetach();
        }
        m_Layers.clear();
    }

    void LayerStack::PushLayer(Ref<Layer> layer) {
        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
        m_LayerInsertIndex++;
        layer->OnAttach();
    }

    void LayerStack::PushOverlay(Ref<Layer> overlay) {
        m_Layers.emplace_back(overlay);
        overlay->OnAttach();
    }

    void LayerStack::PopLayer(Ref<Layer> layer) {
        auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, layer);
        if (it != m_Layers.begin() + m_LayerInsertIndex) {
            layer->OnDetach();
            m_Layers.erase(it);
            m_LayerInsertIndex--;
        }
    }

    void LayerStack::PopOverlay(Ref<Layer> overlay) {
        auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), overlay);
        if (it != m_Layers.end()) {
            overlay->OnDetach();
            m_Layers.erase(it);
        }
    }

    bool LayerStack::OnEvent(Event& event) {
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) {
            (*it)->OnEvent(event);
            if (event.Handled) {
                return true;
            }
        }
        return false;
    }
}
