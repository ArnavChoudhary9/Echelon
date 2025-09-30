#pragma once

#include "../Core/Base.h"
#include <iostream>
#include <string>

namespace Echelon {
    /**
     * @brief Represents the various types of events that can occur.
     * 
     */
    enum class EventType {
        None = 0,
        WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
        AppTick, AppUpdate, AppRender,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
    };

    /**
     * @brief Represents the various categories of events.
     * 
     */
    enum EventCategory {
        None = 0,
        EventCategoryApplication    = BIT(1),
        EventCategoryInput          = BIT(2),
        EventCategoryKeyboard       = BIT(3),
        EventCategoryMouse          = BIT(4),
        EventCategoryMouseButton    = BIT(5)
    };

    // Helper macros for defining event types and categories
    #define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; }\
                                EventType GetEventType() const override { return GetStaticType(); }\
                                const char* GetName() const override { return #type; }

    #define EVENT_CLASS_CATEGORY(category) int GetCategoryFlags() const override { return category; }

    class Event {
    public:
        virtual ~Event() = default;

        /**
         * @brief Get the type of the event.
         * 
         * @return EventType 
         */
        virtual EventType GetEventType() const = 0;

        /**
         * @brief Get the name of the event.
         * 
         * @return const char* 
         */
        virtual const char* GetName() const = 0;

        /**
         * @brief Get the category flags of the event.
         * 
         * @return int 
         */
        virtual int GetCategoryFlags() const = 0;

        /**
         * @brief Convert the event to a string representation.
         * 
         * @return std::string 
         */
        virtual std::string ToString() const { return GetName(); }

        /**
         * @brief Check if the event is in a specific category.
         * 
         * @param category The category to check against.
         * @return true If the event is in the specified category.
         * @return false If the event is not in the specified category.
         */
        bool IsInCategory(EventCategory category) {
            return GetCategoryFlags() & category;
        }

        bool Handled = false;
    };

    // Overload the output stream operator for Event
    inline std::ostream& operator<<(std::ostream& os, const Event& e) {
        return os << e.ToString();
    }

    /**
     * @brief Dispatches events to the appropriate event handler based on the event type.
     * 
     */
    class EventDispatcher {
    public:
        /**
         * @brief Construct a new Event Dispatcher object
         * 
         * @param event The event to be dispatched.
         */
        EventDispatcher(Event& event)
            : m_Event(event) {}

        // F will be deduced by the compiler
        template<typename T, typename F>
        bool Dispatch(const F& func) {
            if (m_Event.GetEventType() == T::GetStaticType()) {
                m_Event.Handled = func(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

    private:
        Event& m_Event;
    };
}
