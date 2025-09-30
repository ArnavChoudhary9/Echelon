#pragma once

namespace Echelon {
    class Layer {
    public:
        virtual ~Layer() = default;

        virtual void OnAttach() = 0;
        virtual void OnDetach() = 0;
        virtual void OnUpdate(float) = 0;
    };
}
