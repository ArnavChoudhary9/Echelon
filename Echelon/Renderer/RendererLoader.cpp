#include "RendererLoader.hpp"
#include "Core/Log.hpp"

#include <utility>

#if defined(_WIN32) || defined(_WIN64)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #include <mach-o/dyld.h>
    #include <climits>
#else
    #include <dlfcn.h>
#endif

namespace Echelon {

    // ------------------------------------------------------------------
    // Platform helpers
    // ------------------------------------------------------------------

    fs::path RendererLoader::ExecutableDir() {
#if defined(_WIN32) || defined(_WIN64)
        char buf[MAX_PATH];
        DWORD n = ::GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
        if (n > 0 && n < sizeof(buf))
            return fs::path(buf).parent_path();
        return fs::current_path();
#elif defined(__APPLE__)
        // _NSGetExecutablePath gives the path of the running executable.
        char buf[PATH_MAX];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
            return fs::canonical(buf).parent_path();
        return fs::current_path();
#else
        // /proc/self/exe is the running binary on Linux.
        return fs::canonical("/proc/self/exe").parent_path();
#endif
    }

    fs::path RendererLoader::ResolveLibraryPath(const std::string& baseName) {
        // Build an absolute path so dlopen treats it as a path (contains '/')
        // and skips the system library search entirely.
#if defined(_WIN32) || defined(_WIN64)
        return ExecutableDir() / (baseName + ".dll");
#elif defined(__APPLE__)
        return ExecutableDir() / ("lib" + baseName + ".dylib");
#else
        return ExecutableDir() / ("lib" + baseName + ".so");
#endif
    }

    static void* PlatformLoadLibrary(const fs::path& path) {
#if defined(_WIN32) || defined(_WIN64)
        return static_cast<void*>(::LoadLibraryA(path.string().c_str()));
#else
        return ::dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    }

    static void PlatformFreeLibrary(void* handle) {
        if (!handle) return;
#if defined(_WIN32) || defined(_WIN64)
        ::FreeLibrary(static_cast<HMODULE>(handle));
#else
        ::dlclose(handle);
#endif
    }

    static void* PlatformGetSymbol(void* handle, const char* name) {
#if defined(_WIN32) || defined(_WIN64)
        return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), name));
#else
        return ::dlsym(handle, name);
#endif
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    RendererLoader::~RendererLoader() {
        Unload();
    }

    RendererLoader::RendererLoader(RendererLoader&& other) noexcept
        : m_DLLHandle(other.m_DLLHandle),
          m_Renderer(other.m_Renderer),
          m_DestroyFn(other.m_DestroyFn),
          m_Info(std::move(other.m_Info)),
          m_Path(std::move(other.m_Path))
    {
        other.m_DLLHandle = nullptr;
        other.m_Renderer  = nullptr;
        other.m_DestroyFn = nullptr;
    }

    RendererLoader& RendererLoader::operator=(RendererLoader&& other) noexcept {
        if (this != &other) {
            Unload();
            m_DLLHandle      = other.m_DLLHandle;
            m_Renderer       = other.m_Renderer;
            m_DestroyFn      = other.m_DestroyFn;
            m_Info           = std::move(other.m_Info);
            m_Path           = std::move(other.m_Path);
            other.m_DLLHandle = nullptr;
            other.m_Renderer  = nullptr;
            other.m_DestroyFn = nullptr;
        }
        return *this;
    }

    // ------------------------------------------------------------------
    // Load
    // ------------------------------------------------------------------

    bool RendererLoader::Load(const fs::path& dllPath) {
        // Unload any previously loaded renderer
        if (m_DLLHandle) {
            Unload();
        }

        ECHELON_LOG_INFO("[RendererLoader] Loading renderer plugin: {}", dllPath.string());

        m_DLLHandle = PlatformLoadLibrary(dllPath);
        if (!m_DLLHandle) {
            ECHELON_LOG_ERROR("[RendererLoader] Failed to load DLL: {}", dllPath.string());
            return false;
        }

        // Resolve factory symbols
        auto createFn = reinterpret_cast<CreateRendererFn>(
            PlatformGetSymbol(m_DLLHandle, "CreateRenderer"));
        m_DestroyFn = reinterpret_cast<DestroyRendererFn>(
            PlatformGetSymbol(m_DLLHandle, "DestroyRenderer"));

        if (!createFn) {
            ECHELON_LOG_ERROR("[RendererLoader] Missing 'CreateRenderer' export in {}", dllPath.string());
            PlatformFreeLibrary(m_DLLHandle);
            m_DLLHandle = nullptr;
            return false;
        }

        if (!m_DestroyFn) {
            ECHELON_LOG_ERROR("[RendererLoader] Missing 'DestroyRenderer' export in {}", dllPath.string());
            PlatformFreeLibrary(m_DLLHandle);
            m_DLLHandle = nullptr;
            return false;
        }

        // Create the renderer instance
        m_Renderer = createFn();
        if (!m_Renderer) {
            ECHELON_LOG_ERROR("[RendererLoader] CreateRenderer() returned null");
            PlatformFreeLibrary(m_DLLHandle);
            m_DLLHandle = nullptr;
            m_DestroyFn = nullptr;
            return false;
        }

        m_Path = dllPath;
        m_Info = m_Renderer->GetInfo();
        ECHELON_LOG_INFO("[RendererLoader] Loaded '{}' v{} by {}",
                         m_Info.Name, m_Info.Version, m_Info.Author);

        return true;
    }

    // ------------------------------------------------------------------
    // Unload
    // ------------------------------------------------------------------

    void RendererLoader::Unload() {
        if (m_Renderer && m_DestroyFn) {
            m_Renderer->Shutdown();
            m_DestroyFn(m_Renderer);
            m_Renderer  = nullptr;
            m_DestroyFn = nullptr;
        }

        if (m_DLLHandle) {
            PlatformFreeLibrary(m_DLLHandle);
            m_DLLHandle = nullptr;
        }

        ECHELON_LOG_INFO("[RendererLoader] Renderer unloaded.");
    }

} // namespace Echelon
