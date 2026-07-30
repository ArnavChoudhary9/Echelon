#pragma once

/**
 * @file RendererLoader.hpp
 * @brief Runtime shared-library loader for a single renderer plugin.
 *
 * This is the low-level, per-library RAII wrapper. The engine-facing
 * Renderer service (RendererService.hpp) owns one RendererLoader per loaded
 * plugin and decides which one is active.
 *
 * Best Practices:
 *  - The loader owns the library handle lifetime; Unload() releases it cleanly.
 *  - All calls to the renderer go through Get() which returns a raw
 *    non-owning pointer — the loader manages the object's lifetime.
 *  - Fatal errors (missing library / missing exports) are logged through the
 *    engine logger and leave the renderer in a null state.
 *  - The loader is renderer-agnostic: it never assumes a particular plugin
 *    name. Use ResolveLibraryPath("<name>") to turn a base name into the
 *    platform library path (lib<name>.so / <name>.dll / lib<name>.dylib)
 *    next to the executable.
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
     *     if (loader.Load(RendererLoader::ResolveLibraryPath("Ray"))) {
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
         * Loads the shared library at @p dllPath, resolves
         * CreateRenderer / DestroyRenderer, and instantiates the renderer.
         * Use ResolveLibraryPath("<name>") to build a path from a base name.
         *
         * @param dllPath Absolute (or exe-relative) path of the renderer library.
         * @return true if the renderer was loaded successfully.
         */
        bool Load(const fs::path& dllPath);

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

        /**
         * @brief Plugin-reported metadata (valid after a successful Load()).
         */
        const RendererInfo& Info() const { return m_Info; }

        /**
         * @brief The resolved library path this loader loaded from.
         */
        const fs::path& Path() const { return m_Path; }

        /**
         * @brief Directory containing the running executable.
         *
         * Cross-platform: uses /proc/self/exe (Linux), _NSGetExecutablePath
         * (macOS), and GetModuleFileName (Windows).
         */
        static fs::path ExecutableDir();

        /**
         * @brief Turn a renderer base name into the platform library path next
         *        to the executable, e.g. "Ray" ->
         *        <exe_dir>/libRay.so | Ray.dll | libRay.dylib.
         */
        static fs::path ResolveLibraryPath(const std::string& baseName);

    private:
        using CreateRendererFn  = RendererAPI* (*)();
        using DestroyRendererFn = void (*)(RendererAPI*);

        void*                 m_DLLHandle = nullptr;
        RendererAPI*          m_Renderer  = nullptr;
        DestroyRendererFn     m_DestroyFn = nullptr;
        RendererInfo          m_Info;
        fs::path m_Path;
    };

} // namespace Echelon
