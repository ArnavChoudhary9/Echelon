// Safety guard: this file is the fallback for platforms without a native watcher.
// Premake's removefiles excludes it on Linux, Windows, and macOS; the guard is a
// belt-and-suspenders fallback in case the file is compiled on a known platform by mistake.
#if !defined(ECHELON_PLATFORM_LINUX) && \
    !defined(ECHELON_PLATFORM_WINDOWS) && \
    !defined(ECHELON_PLATFORM_MACOS)

#include "Platform/FileWatcher.hpp"
#include "Core/Log.hpp"

#include <chrono>
#include <shared_mutex>
#include <unordered_map>

namespace Echelon {

    struct FileWatcher::Impl {
        mutable std::shared_mutex m_WatchMutex;
        // path string → last-seen mtime
        std::unordered_map<std::string, fs::file_time_type> m_Watched;
    };

    FileWatcher::FileWatcher()  : m_Impl(std::make_unique<Impl>()) {}
    FileWatcher::~FileWatcher() = default;

    void FileWatcher::Start() {
        m_Running.store(true);
        m_Thread = std::thread(&FileWatcher::ThreadFunc, this);
        ECHELON_LOG_WARN("[FileWatcher] No native OS watcher available; "
                         "using portable 250 ms poll fallback.");
    }

    void FileWatcher::Stop() {
        m_Running.store(false);
        if (m_Thread.joinable())
            m_Thread.join();

        std::unique_lock lk(m_Impl->m_WatchMutex);
        m_Impl->m_Watched.clear();
    }

    void FileWatcher::Watch(const fs::path& filePath) {
        const std::string key = filePath.string();

        std::unique_lock lk(m_Impl->m_WatchMutex);
        if (m_Impl->m_Watched.find(key) != m_Impl->m_Watched.end()) return;

        std::error_code ec;
        auto t = fs::last_write_time(filePath, ec);
        if (!ec)
            m_Impl->m_Watched[key] = t;
    }

    void FileWatcher::Unwatch(const fs::path& filePath) {
        std::unique_lock lk(m_Impl->m_WatchMutex);
        m_Impl->m_Watched.erase(filePath.string());
    }

    void FileWatcher::ThreadFunc() {
        using namespace std::chrono_literals;
        while (m_Running.load()) {
            std::this_thread::sleep_for(250ms);

            std::unique_lock lk(m_Impl->m_WatchMutex);
            for (auto& [pathStr, lastTime] : m_Impl->m_Watched) {
                std::error_code ec;
                auto now = fs::last_write_time(fs::path(pathStr), ec);
                if (!ec && now != lastTime) {
                    lastTime = now;
                    std::lock_guard ql(m_QueueMutex);
                    m_Queue.push_back(fs::path(pathStr));
                }
            }
        }
    }

} // namespace Echelon

#endif // !LINUX && !WINDOWS && !MACOS
