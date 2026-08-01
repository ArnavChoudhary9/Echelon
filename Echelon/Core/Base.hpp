#pragma once

#include <cstdint>
#include <memory>
#include <filesystem>

// ---- Symbol visibility / DLL export-import ----
// ENGINE_API decorates symbols that cross the engine shared-library boundary.
//
// Usage:
//   class ENGINE_API MyClass { ... };
//   ENGINE_API void MyFunc();
//
// Build-system rules (set by premake):
//   Building the engine DLL  → ECHELON_BUILD_DLL is defined → dllexport / visibility("default")
//   Consuming the engine DLL → neither defined               → dllimport  / visibility("default")
//
// On Linux / macOS all symbols are visible by default, so the attribute is
// a no-op in practice; it documents intent and keeps the API surface explicit.
#if defined(ECHELON_PLATFORM_WINDOWS)
    #ifdef ECHELON_BUILD_DLL
        #define ENGINE_API __declspec(dllexport)
    #else
        #define ENGINE_API __declspec(dllimport)
    #endif
#else
    #define ENGINE_API __attribute__((visibility("default")))
#endif

namespace Echelon {
    /**
     * @brief Engine-wide shorthand for the standard filesystem namespace.
     *
     * Use `fs::path` (and `fs::exists`, `fs::create_directories`, ...) throughout
     * the engine instead of the verbose `std::filesystem::` spelling. All
     * filesystem paths should be represented as `fs::path` rather than strings.
     */
    namespace fs = std::filesystem;

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
    
    template<typename T>
    using WeakRef = std::weak_ptr<T>;
    template<typename T, typename... Args>
    constexpr WeakRef<T> CreateWeakRef(const Ref<T>& ref) {
        return std::weak_ptr<T>(ref);
    }

    // Simple struct to hold width and height dimensions
    struct Dimension {
        uint32_t Width;
        uint32_t Height;
    };
}
