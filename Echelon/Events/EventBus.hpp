#pragma once

/**
 * @file EventBus.hpp
 * @brief Engine-wide publish/subscribe message bus (core functionality).
 *
 * Decoupled, many-to-many messaging over ARBITRARY types: a message is any
 * copyable struct/class — no base class, macros, or registration required. This
 * is deliberately distinct from the input Event / EventDispatcher system in
 * Event.hpp (which propagates window/input events *down the layer stack* with a
 * Handled flag). The bus is for decoupled signals and commands between systems
 * that should not know about each other directly (editor panels, tools, gameplay
 * systems, asset pipeline, ...).
 *
 * Dispatch is synchronous: Publish<T>(msg) invokes every T-subscriber inline, in
 * subscription order, before returning. Subscribers hand back a SubscriptionId;
 * prefer ScopedSubscription (RAII) so a handler can never outlive the object that
 * owns it — a dangling std::function would crash on the next Publish.
 */

#include "Echelon/Core/Base.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Echelon {

    using SubscriptionId = uint64_t;

    class EventBus {
    public:
        /** @brief The process-wide bus. Anything may Get() it to publish/subscribe. */
        static EventBus& Get() {
            static EventBus s_Instance;
            return s_Instance;
        }

        /**
         * @brief Register a handler for messages of type @p T.
         * @return An id that can be passed to Unsubscribe(). Prefer ScopedSubscription.
         */
        template<typename T>
        SubscriptionId Subscribe(std::function<void(const T&)> fn) {
            const SubscriptionId id = ++m_NextId;
            auto& list = m_Handlers[std::type_index(typeid(T))];
            list.push_back(Entry{ id, [fn = std::move(fn)](const void* payload) {
                fn(*static_cast<const T*>(payload));
            }});
            return id;
        }

        /** @brief Remove a handler by id. No-op if it was already removed. */
        void Unsubscribe(SubscriptionId id) {
            for (auto& [type, list] : m_Handlers) {
                const auto it = std::remove_if(list.begin(), list.end(),
                    [id](const Entry& e) { return e.Id == id; });
                if (it != list.end()) {
                    list.erase(it, list.end());
                    return;
                }
            }
        }

        /**
         * @brief Deliver @p msg to every subscriber of type @p T, in subscription order.
         *
         * The handler list is snapshotted before dispatch so a handler that
         * (un)subscribes while being invoked cannot invalidate the walk.
         */
        template<typename T>
        void Publish(const T& msg) {
            const auto it = m_Handlers.find(std::type_index(typeid(T)));
            if (it == m_Handlers.end() || it->second.empty()) return;
            const std::vector<Entry> snapshot = it->second;
            for (const auto& e : snapshot)
                e.Fn(&msg);
        }

    private:
        struct Entry {
            SubscriptionId                     Id;
            std::function<void(const void*)>   Fn;
        };

        EventBus() = default;

        std::unordered_map<std::type_index, std::vector<Entry>> m_Handlers;
        SubscriptionId m_NextId = 0;
    };

    /**
     * @brief RAII subscription handle — unsubscribes on destruction.
     *
     * Hold one as a member in any subscriber so its handler's lifetime is tied to
     * the object. Movable (transfers ownership), non-copyable.
     */
    class ScopedSubscription {
    public:
        ScopedSubscription() = default;

        template<typename T>
        ScopedSubscription(EventBus& bus, std::function<void(const T&)> fn)
            : m_Bus(&bus), m_Id(bus.Subscribe<T>(std::move(fn))) {}

        ~ScopedSubscription() { Reset(); }

        ScopedSubscription(ScopedSubscription&& other) noexcept { *this = std::move(other); }
        ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
            if (this != &other) {
                Reset();
                m_Bus = other.m_Bus;
                m_Id  = other.m_Id;
                other.m_Bus = nullptr;
                other.m_Id  = 0;
            }
            return *this;
        }

        ScopedSubscription(const ScopedSubscription&)            = delete;
        ScopedSubscription& operator=(const ScopedSubscription&) = delete;

        void Reset() {
            if (m_Bus && m_Id) m_Bus->Unsubscribe(m_Id);
            m_Bus = nullptr;
            m_Id  = 0;
        }

    private:
        EventBus*      m_Bus = nullptr;
        SubscriptionId m_Id  = 0;
    };

    /**
     * @brief Convenience: subscribe on the global bus and get an RAII handle back.
     *        Usage: m_Sub = OnMessage<EntitySelectedEvent>([this](auto& e){ ... });
     */
    template<typename T, typename F>
    [[nodiscard]] ScopedSubscription OnMessage(F&& fn) {
        return ScopedSubscription(EventBus::Get(), std::function<void(const T&)>(std::forward<F>(fn)));
    }

    /** @brief Convenience: publish on the global bus. */
    template<typename T>
    void PublishEvent(const T& msg) {
        EventBus::Get().Publish<T>(msg);
    }

} // namespace Echelon
