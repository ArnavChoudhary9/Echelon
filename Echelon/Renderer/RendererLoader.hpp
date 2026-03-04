#pragma once

/**
 * @file RendererLoader.hpp
 * @brief Runtime DLL loader for the renderer plugin (Renderer.dll).
 *
 * Best Practices:
 *  - Only one renderer is active at a time; call Load() once during startup.
 *  - The loader owns the DLL handle lifetime; Unload() releases it cleanly.
 *  - All calls to the renderer go through Get() which returns a raw
 *    non-owning pointer — the loader manages the object's lifetime.
 *  - Fatal errors (missing DLL / missing exports) are logged through the
 *    engine logger and leave the renderer in a null state.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/Renderer/RendererAPI.hpp"

#include <string>
#include <filesystem>

namespace Echelon {

    /**
     * @brief Loads and manages the renderer plugin DLL at runtime.
     *
     * Usage:
     * @code
     *     RendererLoader loader;
     *     if (loader.Load("Renderer.dll")) {
     *         auto* api = loader.Get();
     *         api->Init(hwnd, w, h);
     *     }
     *     // ... at shutdown
     *     loader.Unload();
     * @endcode
     */
    class RendererLoader {
    public:
        RendererLoader() = default;
        ~RendererLoader();

        // Non-copyable, movable
        RendererLoader(const RendererLoader&) = delete;
        RendererLoader& operator=(const RendererLoader&) = delete;
        RendererLoader(RendererLoader&& other) noexcept;
        RendererLoader& operator=(RendererLoader&& other) noexcept;

        /**
         * @brief Load a renderer plugin.
         *
         * Searches for the DLL in the executable's directory (or a supplied path),
         * resolves CreateRenderer / DestroyRenderer, and instantiates the renderer.
         *
         * @param dllPath Path or filename of the renderer DLL
         *                (default: "Renderer.dll" / "libRenderer.so").
         * @return true if the renderer was loaded successfully.
         */
        bool Load(const std::filesystem::path& dllPath = DefaultDLLName());

        /**
         * @brief Unload the renderer, destroying the instance and releasing the DLL.
         */
        void Unload();

        /**
         * @brief Check whether a renderer is currently loaded.
         */
        bool IsLoaded() const { return m_Renderer != nullptr; }

        /**
         * @brief Get the active renderer instance.
         * @return RendererAPI* Non-owning pointer (null if not loaded).
         */
        RendererAPI* Get() const { return m_Renderer; }

        /**
         * @brief Arrow operator for convenience.
         */
        RendererAPI* operator->() const { return m_Renderer; }

    private:
        static std::filesystem::path DefaultDLLName();

        using CreateRendererFn  = RendererAPI* (*)();
        using DestroyRendererFn = void (*)(RendererAPI*);

        void*              m_DLLHandle      = nullptr;
        RendererAPI*       m_Renderer       = nullptr;
        DestroyRendererFn  m_DestroyFn      = nullptr;
    };

} // namespace Echelon
