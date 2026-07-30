#pragma once

/**
 * @file RendererService.hpp
 * @brief Global renderer service — owns every loaded renderer plugin and the
 *        currently-active one, and supports on-the-fly hot-swapping.
 *
 * The engine loads a default renderer (chosen at build time — see
 * ECHELON_DEFAULT_RENDERER / the premake `--renderer` option) during
 * Application startup and exposes it here through a singleton. User code never
 * touches a RendererLoader directly; it goes through `Renderer::Get()`.
 *
 * Design:
 *  - The class is `Renderer` (a singleton) even though the file is named
 *    RendererService.hpp — the file name only avoids a clash with the umbrella
 *    header Renderer/Renderer.hpp.
 *  - A registry maps a logical name → RendererLoader (which owns the dlopen
 *    handle + the RendererAPI instance created by the plugin's CreateRenderer).
 *  - "Loaded" (dlopen'd, instance created) is distinct from "active"
 *    (its Init() has run and it owns the live GL resources). Because the
 *    OpenGL backend has a single GL context bound to the window, only ONE
 *    renderer is ever initialised at a time. Swapping = Shutdown old →
 *    Init new → notify listeners.
 *  - Change listeners fire on every swap AND immediately on subscription if a
 *    renderer is already active (replay-on-subscribe), so user code can keep
 *    all GPU-resource loading in one place that reruns on each swap.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/Renderer/RendererAPI.hpp"
#include "Echelon/Renderer/RendererLoader.hpp"

#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <unordered_map>

namespace Echelon {

    class Window; // fwd — implementation pulls in Platform/Window.hpp

    /**
     * @brief Invoked whenever the active renderer changes.
     * @param newActive The now-active renderer, or nullptr on shutdown/failure.
     */
    using RendererChangedCallback = std::function<void(RendererAPI* newActive)>;

    /**
     * @brief Global renderer registry + active-renderer manager (singleton).
     */
    class Renderer {
    public:
        /** @brief Access the singleton instance. */
        static Renderer& Get();

        // ---- Engine-internal lifecycle (called by Application) ----

        /**
         * @brief Load the default renderer, initialise it against the window,
         *        and make it active.
         * @param window       The platform window (provides native handle + size).
         * @param defaultName  Logical name / library base name of the default
         *                     renderer (renderer-agnostic; e.g. "Ray").
         * @return true if the default renderer was loaded and activated.
         */
        bool Init(Window& window, const std::string& defaultName);

        /**
         * @brief Shut down the active renderer and unload every loaded plugin.
         *        Fires the change callback with nullptr.
         */
        void Shutdown();

        // ---- Loading / unloading (does NOT initialise GL) ----

        /**
         * @brief dlopen a renderer library and instantiate it (no Init yet).
         * @param name Logical registry key.
         * @param path Library path. If empty, the base name `name` is resolved
         *             to a platform library next to the executable
         *             (lib<name>.so / <name>.dll / lib<name>.dylib).
         * @return true if the library loaded and the instance was created.
         */
        bool LoadRenderer(const std::string& name,
                          const fs::path& path = {});

        /**
         * @brief Destroy an instance and dlclose its library.
         *        Refuses to unload the currently-active renderer.
         */
        void UnloadRenderer(const std::string& name);

        // ---- Active-renderer selection (hot swap) ----

        /**
         * @brief Make a loaded renderer active: shut down the current active
         *        one, initialise the target against the window, then notify
         *        listeners. Must already be LoadRenderer()'d.
         * @return true on success; on failure leaves no active renderer.
         */
        bool SetActive(const std::string& name);

        /** @brief The active renderer, or nullptr if none. */
        RendererAPI* GetActive() const { return m_Active; }

        /** @brief Convenience: forward directly to the active renderer. */
        RendererAPI* operator->() const { return m_Active; }

        /** @brief Whether a renderer is currently active. */
        bool HasActive() const { return m_Active != nullptr; }

        /** @brief Name of the active renderer (empty if none). */
        const std::string& GetActiveName() const { return m_ActiveName; }

        // ---- Introspection ----

        /** @brief Whether a renderer with this name is loaded. */
        bool IsLoaded(const std::string& name) const;

        /** @brief Names of all loaded renderers. */
        std::vector<std::string> List() const;

        /** @brief Plugin-reported info for a loaded renderer (empty if absent). */
        RendererInfo GetInfo(const std::string& name) const;

        // ---- Convenience forwarding (used by user code each frame) ----

        /** @brief Cache the new size and forward to the active renderer. */
        void OnResize(uint32_t width, uint32_t height);

        /** @brief Cache the VSync preference and forward to the active renderer. */
        void SetVSync(bool enabled);

        /** @brief Query the cached VSync preference. */
        bool IsVSync() const { return m_VSync; }

        // ---- Change notification ----

        /**
         * @brief Register a callback fired when the active renderer changes.
         *
         * If a renderer is already active, @p cb is invoked immediately with it
         * (replay-on-subscribe) — this is what delivers the "startup" fire to
         * listeners that subscribe after Init().
         * @return An id usable with RemoveChangeListener.
         */
        uint32_t AddChangeListener(RendererChangedCallback cb);

        /** @brief Remove a previously registered change listener. */
        void RemoveChangeListener(uint32_t id);

    private:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        struct Entry {
            Scope<RendererLoader> Loader;      ///< owns dlopen handle + instance
            RendererInfo          Info;        ///< cached plugin metadata
            bool                  Initialized = false;
        };

        Entry* Find(const std::string& name);
        const Entry* Find(const std::string& name) const;
        void NotifyChanged();

        std::unordered_map<std::string, Entry> m_Renderers;  ///< registry of all loaded libs
        std::string  m_ActiveName;
        RendererAPI* m_Active = nullptr;                      ///< == m_Renderers[m_ActiveName].Loader->Get()

        // Window state threaded through so swaps can re-Init without the caller re-supplying it.
        void*    m_WindowHandle = nullptr;
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        bool     m_VSync  = true;

        std::vector<std::pair<uint32_t, RendererChangedCallback>> m_Listeners;
        uint32_t m_NextListenerId = 1;
    };

} // namespace Echelon
