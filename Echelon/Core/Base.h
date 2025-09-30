#pragma once

#include <memory>

namespace Echelon {
    /**
     * @brief Creates a bitmask with a 1 at the specified position.
     * 
     * @param x The position of the bit to set (0-indexed).
     * @return int The bitmask with the bit at position x set to 1.
     */
    #define BIT(x) (1 << x)

    /**
     * @brief Binds a member function to the current instance, allowing it to be used as a callback.
     * 
     * @param fn The member function to bind.
     * @return A lambda function that captures 'this' and forwards arguments to the member function.
     */
    #define EH_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

    template<typename T>
    using Ref = std::shared_ptr<T>;
    template<typename T, typename... Args>
    constexpr Ref<T> CreateRef(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    template<typename T>
    using Scope = std::unique_ptr<T>;
    template<typename T, typename... Args>
    constexpr Scope<T> CreateScope(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
}
