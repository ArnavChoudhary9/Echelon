#pragma once

#include "Event.h"

namespace Echelon {
    /**
     * @brief Represents application-specific events.
     * 
     */
    class ApplicationEvent : public Event {
    public:
        virtual ~ApplicationEvent() = default;
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class AppTickEvent : public ApplicationEvent {
    public:
        AppTickEvent() = default;
        EVENT_CLASS_TYPE(AppTick)
    };

    class AppUpdateEvent : public ApplicationEvent {
    public:
        AppUpdateEvent() = default;
        EVENT_CLASS_TYPE(AppUpdate)
    };

    class AppRenderEvent : public ApplicationEvent {
    public:
        AppRenderEvent() = default;
        EVENT_CLASS_TYPE(AppRender)
    };

    // Window Events

    class WindowCloseEvent : public ApplicationEvent {
    public:
        WindowCloseEvent() = default;
        EVENT_CLASS_TYPE(WindowClose)
    };

    class WindowResizeEvent : public ApplicationEvent {
    public:
        WindowResizeEvent(int width, int height)
            : m_Width(width), m_Height(height) {}
        
        EVENT_CLASS_TYPE(WindowResize)

        std::string ToString() const override {
            return std::string(GetName()) + ": " + std::to_string(m_Width) + ", " + std::to_string(m_Height);
        }

        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
    private:
        int m_Width, m_Height;
    };

    class WindowFocusEvent : public ApplicationEvent {
    public:
        WindowFocusEvent() = default;
        EVENT_CLASS_TYPE(WindowFocus)
    };

    class WindowLostFocusEvent : public ApplicationEvent {
    public:
        WindowLostFocusEvent() = default;
        EVENT_CLASS_TYPE(WindowLostFocus)
    };

    class WindowMovedEvent : public ApplicationEvent {
    public:
        WindowMovedEvent(int x, int y)
            : m_X(x), m_Y(y) {}
        
        EVENT_CLASS_TYPE(WindowMoved)

        std::string ToString() const override {
            return std::string(GetName()) + ": " + std::to_string(m_X) + ", " + std::to_string(m_Y);
        }

        int GetX() const { return m_X; }
        int GetY() const { return m_Y; }
    private:
        int m_X, m_Y;
    };
}
