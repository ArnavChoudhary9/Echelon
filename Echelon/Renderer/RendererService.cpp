#include "RendererService.hpp"

#include "Core/Log.hpp"
#include "Echelon/Platform/Window.hpp"

#include <algorithm>
#include <utility>

namespace Echelon {

    // ------------------------------------------------------------------
    // Singleton
    // ------------------------------------------------------------------

    Renderer& Renderer::Get() {
        static Renderer s_Instance;
        return s_Instance;
    }

    Renderer::~Renderer() {
        // Deterministic teardown is the caller's job (Application invokes
        // Shutdown() while the window/GL context is still alive). If the
        // registry is empty here, there is nothing to do; if it is not, the
        // GL context is already gone and we must not touch it — so this is a
        // no-op by design.
    }

    // ------------------------------------------------------------------
    // Registry helpers
    // ------------------------------------------------------------------

    Renderer::Entry* Renderer::Find(const std::string& name) {
        auto it = m_Renderers.find(name);
        return it == m_Renderers.end() ? nullptr : &it->second;
    }

    const Renderer::Entry* Renderer::Find(const std::string& name) const {
        auto it = m_Renderers.find(name);
        return it == m_Renderers.end() ? nullptr : &it->second;
    }

    // ------------------------------------------------------------------
    // Engine lifecycle
    // ------------------------------------------------------------------

    bool Renderer::Init(Window& window, const std::string& defaultName) {
        // Cache the window state so swaps can re-Init without re-supplying it.
        m_WindowHandle = window.GetNativeHandle();
        m_Width        = window.GetWidth();
        m_Height       = window.GetHeight();
        m_VSync        = window.IsVSync();

        if (!LoadRenderer(defaultName)) {
            ECHELON_LOG_FATAL("[Renderer] PANIC: default renderer '{}' is not present; "
                              "no renderer available.", defaultName);
            return false;
        }

        if (!SetActive(defaultName)) {
            ECHELON_LOG_FATAL("[Renderer] PANIC: default renderer '{}' failed to activate.",
                              defaultName);
            return false;
        }

        ECHELON_LOG_INFO("[Renderer] Default renderer '{}' active.", defaultName);
        return true;
    }

    void Renderer::Shutdown() {
        if (m_Active) {
            ECHELON_LOG_INFO("[Renderer] Shutting down active renderer '{}'", m_ActiveName);
            m_Active->Shutdown();
            if (Entry* active = Find(m_ActiveName))
                active->Initialized = false;
            m_Active = nullptr;
            m_ActiveName.clear();
            NotifyChanged();
        }

        // Destroying each Entry runs ~RendererLoader → DestroyRenderer + dlclose.
        m_Renderers.clear();
    }

    // ------------------------------------------------------------------
    // Loading / unloading
    // ------------------------------------------------------------------

    bool Renderer::LoadRenderer(const std::string& name, const std::filesystem::path& path) {
        if (IsLoaded(name)) {
            ECHELON_LOG_WARN("[Renderer] Renderer '{}' is already loaded; ignoring.", name);
            return true;
        }

        // Resolve the library path.
        std::filesystem::path resolved;
        if (path.empty())
            resolved = RendererLoader::ResolveLibraryPath(name);
        else if (path.is_relative())
            resolved = RendererLoader::ExecutableDir() / path;
        else
            resolved = path;

        auto loader = CreateScope<RendererLoader>();
        if (!loader->Load(resolved)) {
            // Library missing, or present but missing the required exports
            // (incompatible). RendererLoader has already logged the specifics.
            ECHELON_LOG_FATAL("[Renderer] PANIC: renderer '{}' could not be loaded "
                              "from {} (missing or incompatible).",
                              name, resolved.string());
            return false;
        }

        Entry entry;
        entry.Info   = loader->Info();
        entry.Loader = std::move(loader);

        // Log before the entry is moved into the registry.
        ECHELON_LOG_INFO("[Renderer] Registered renderer '{}' ('{}' v{})",
                         name, entry.Info.Name, entry.Info.Version);

        m_Renderers.emplace(name, std::move(entry));
        return true;
    }

    void Renderer::UnloadRenderer(const std::string& name) {
        if (name == m_ActiveName) {
            ECHELON_LOG_ERROR("[Renderer] Cannot unload active renderer '{}'; "
                              "SetActive to another renderer first.", name);
            return;
        }

        if (!Find(name)) {
            ECHELON_LOG_WARN("[Renderer] Unload requested for unknown renderer '{}'", name);
            return;
        }

        // Erasing runs ~RendererLoader → Shutdown + DestroyRenderer + dlclose.
        m_Renderers.erase(name);
        ECHELON_LOG_INFO("[Renderer] Unloaded renderer '{}'", name);
    }

    // ------------------------------------------------------------------
    // Active-renderer selection (hot swap)
    // ------------------------------------------------------------------

    bool Renderer::SetActive(const std::string& name) {
        if (m_Active && name == m_ActiveName)
            return true; // already active

        Entry* target = Find(name);
        if (!target) {
            // Not loaded (library missing / never registered). Keep the current
            // renderer active — nothing was torn down — and report the failure.
            ECHELON_LOG_ERROR("[Renderer] Cannot activate '{}': not loaded. "
                              "Staying on '{}'.", name,
                              m_ActiveName.empty() ? "<none>" : m_ActiveName);
            return false;
        }

        // Remember the current renderer so we can fall back if the new one
        // fails to initialise (missing/incompatible GPU resources, etc.).
        const std::string prevName   = m_ActiveName;
        const bool        hadPrevious = (m_Active != nullptr);

        // Release the currently-active renderer's GL resources (the GL context
        // itself belongs to the Window and survives).
        if (m_Active) {
            m_Active->Shutdown();
            if (Entry* old = Find(m_ActiveName))
                old->Initialized = false;
            m_Active = nullptr;
            m_ActiveName.clear();
        }

        RendererAPI* api = target->Loader->Get();
        if (api && api->Init(m_WindowHandle, m_Width, m_Height)) {
            api->SetVSync(m_VSync);
            target->Initialized = true;
            m_Active     = api;
            m_ActiveName = name;

            ECHELON_LOG_INFO("[Renderer] Active renderer is now '{}'", name);
            NotifyChanged();
            return true;
        }

        // ---- Activation failed: panic + fall back to the last working one ----
        ECHELON_LOG_FATAL("[Renderer] PANIC: renderer '{}' is present but failed "
                          "to initialise (incompatible?).", name);

        if (hadPrevious) {
            if (Entry* prev = Find(prevName)) {
                RendererAPI* prevApi = prev->Loader->Get();
                if (prevApi && prevApi->Init(m_WindowHandle, m_Width, m_Height)) {
                    prevApi->SetVSync(m_VSync);
                    prev->Initialized = true;
                    m_Active     = prevApi;
                    m_ActiveName = prevName;
                    ECHELON_LOG_WARN("[Renderer] Fell back to last working renderer '{}'.",
                                     prevName);
                    NotifyChanged();
                    return false;
                }
            }
            ECHELON_LOG_FATAL("[Renderer] PANIC: fallback to '{}' also failed; "
                              "no active renderer.", prevName);
        }

        // Nothing to fall back to.
        m_ActiveName.clear();
        NotifyChanged(); // listeners see nullptr and can drop stale GPU refs
        return false;
    }

    // ------------------------------------------------------------------
    // Introspection
    // ------------------------------------------------------------------

    bool Renderer::IsLoaded(const std::string& name) const {
        return Find(name) != nullptr;
    }

    std::vector<std::string> Renderer::List() const {
        std::vector<std::string> names;
        names.reserve(m_Renderers.size());
        for (const auto& [name, entry] : m_Renderers)
            names.push_back(name);
        return names;
    }

    RendererInfo Renderer::GetInfo(const std::string& name) const {
        if (const Entry* e = Find(name))
            return e->Info;
        return {};
    }

    // ------------------------------------------------------------------
    // Convenience forwarding
    // ------------------------------------------------------------------

    void Renderer::OnResize(uint32_t width, uint32_t height) {
        m_Width  = width;
        m_Height = height;
        if (m_Active)
            m_Active->OnResize(width, height);
    }

    void Renderer::SetVSync(bool enabled) {
        m_VSync = enabled;
        if (m_Active)
            m_Active->SetVSync(enabled);
    }

    // ------------------------------------------------------------------
    // Change notification
    // ------------------------------------------------------------------

    uint32_t Renderer::AddChangeListener(RendererChangedCallback cb) {
        uint32_t id = m_NextListenerId++;
        m_Listeners.emplace_back(id, std::move(cb));

        // Replay-on-subscribe: deliver the current active renderer immediately
        // so all listeners load their GPU resources in one place, once at
        // startup and again on every swap.
        if (m_Active)
            m_Listeners.back().second(m_Active);

        return id;
    }

    void Renderer::RemoveChangeListener(uint32_t id) {
        m_Listeners.erase(
            std::remove_if(m_Listeners.begin(), m_Listeners.end(),
                           [id](const auto& pair) { return pair.first == id; }),
            m_Listeners.end());
    }

    void Renderer::NotifyChanged() {
        // Snapshot so a listener that (un)subscribes during the callback does
        // not invalidate the iteration.
        auto listeners = m_Listeners;
        for (auto& [id, cb] : listeners) {
            if (cb)
                cb(m_Active);
        }
    }

} // namespace Echelon
