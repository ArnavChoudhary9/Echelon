#pragma once

#include "Event.h"
#include "../Core/MouseCodes.h"

namespace Echelon {
    class MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(float x, float y)
            : m_MouseX(x), m_MouseY(y) {}

        inline float GetX() const { return m_MouseX; }
        inline float GetY() const { return m_MouseY; }

        std::string ToString() const {
            return std::string(GetName()) + ": " + std::to_string(m_MouseX) + ", " + std::to_string(m_MouseY);
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float m_MouseX, m_MouseY;
    };

    class MouseScrolledEvent : public Event {
    public:
        MouseScrolledEvent(float xOffset, float yOffset)
            : m_XOffset(xOffset), m_YOffset(yOffset) {}

        inline float GetXOffset() const { return m_XOffset; }
        inline float GetYOffset() const { return m_YOffset; }

        std::string ToString() const {
            return std::string(GetName()) + ": " + std::to_string(m_XOffset) + ", " + std::to_string(m_YOffset);
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float m_XOffset, m_YOffset;
    };

    class MouseButtonEvent : public Event {
    public:
        inline int GetMouseButton() const { return m_Button; }
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)
    
    protected:
        MouseButtonEvent(const MouseCode button)
            : m_Button(button) {}

        MouseCode m_Button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent {
    public:
        MouseButtonPressedEvent(MouseCode button)
            : MouseButtonEvent(button) {}

        std::string ToString() const {
            return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_Button));
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent {
    public:
        MouseButtonReleasedEvent(MouseCode button)
            : MouseButtonEvent(button) {}

        std::string ToString() const {
            return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_Button));
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };
}
