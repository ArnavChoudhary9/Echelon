// Safety guard: this file must only be compiled on macOS.
// Premake's removefiles already ensures this; the guard is a belt-and-suspenders fallback.
#if defined(ECHELON_PLATFORM_MACOS)

#include "Platform/FileWatcher.hpp"
#include "Core/Log.hpp"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <shared_mutex>
#include <unordered_map>

namespace Echelon {

    struct FileWatcher::Impl {
        int m_Kqueue = -1;

        mutable std::shared_mutex m_WatchMutex;

        // open O_EVTONLY fd → file path
        std::unordered_map<int, fs::path>    m_FdToPath;
        // file path string → open fd
        std::unordered_map<std::string, int> m_PathToFd;
    };

    FileWatcher::FileWatcher()  : m_Impl(std::make_unique<Impl>()) {}
    FileWatcher::~FileWatcher() = default;

    void FileWatcher::Start() {
        m_Impl->m_Kqueue = kqueue();
        if (m_Impl->m_Kqueue < 0) {
            ECHELON_LOG_ERROR("[FileWatcher] kqueue() failed: {}", std::strerror(errno));
            return;
        }
        m_Running.store(true);
        m_Thread = std::thread(&FileWatcher::ThreadFunc, this);
    }

    void FileWatcher::Stop() {
        m_Running.store(false);
        if (m_Thread.joinable())
            m_Thread.join();

        std::unique_lock lk(m_Impl->m_WatchMutex);
        for (auto& [fd, path] : m_Impl->m_FdToPath)
            close(fd);
        m_Impl->m_FdToPath.clear();
        m_Impl->m_PathToFd.clear();

        if (m_Impl->m_Kqueue >= 0) {
            close(m_Impl->m_Kqueue);
            m_Impl->m_Kqueue = -1;
        }
    }

    void FileWatcher::Watch(const fs::path& filePath) {
        if (m_Impl->m_Kqueue < 0) return;

        const std::string key = filePath.string();

        std::unique_lock lk(m_Impl->m_WatchMutex);

        if (m_Impl->m_PathToFd.find(key) != m_Impl->m_PathToFd.end()) return;

        int fd = open(key.c_str(), O_EVTONLY);
        if (fd < 0) {
            ECHELON_LOG_WARN("[FileWatcher] open(O_EVTONLY) failed for '{}': {}",
                             key, std::strerror(errno));
            return;
        }

        struct kevent ev{};
        EV_SET(&ev, fd, EVFILT_VNODE,
               EV_ADD | EV_CLEAR,
               NOTE_WRITE | NOTE_RENAME | NOTE_DELETE,
               0, nullptr);

        if (kevent(m_Impl->m_Kqueue, &ev, 1, nullptr, 0, nullptr) < 0) {
            ECHELON_LOG_WARN("[FileWatcher] kevent(add) failed for '{}': {}",
                             key, std::strerror(errno));
            close(fd);
            return;
        }

        m_Impl->m_FdToPath[fd]  = filePath;
        m_Impl->m_PathToFd[key] = fd;
    }

    void FileWatcher::Unwatch(const fs::path& filePath) {
        if (m_Impl->m_Kqueue < 0) return;

        const std::string key = filePath.string();

        std::unique_lock lk(m_Impl->m_WatchMutex);

        auto it = m_Impl->m_PathToFd.find(key);
        if (it == m_Impl->m_PathToFd.end()) return;

        // Closing the fd removes the kevent filter automatically.
        close(it->second);
        m_Impl->m_FdToPath.erase(it->second);
        m_Impl->m_PathToFd.erase(it);
    }

    void FileWatcher::ThreadFunc() {
        constexpr int MAX_EVENTS = 32;
        struct kevent events[MAX_EVENTS];

        while (m_Running.load()) {
            timespec ts{ 0, 200'000'000L }; // 200 ms timeout
            int nev = kevent(m_Impl->m_Kqueue, nullptr, 0, events, MAX_EVENTS, &ts);
            if (nev <= 0) continue;

            std::shared_lock rl(m_Impl->m_WatchMutex);
            for (int i = 0; i < nev; i++) {
                if (events[i].filter != EVFILT_VNODE) continue;

                auto it = m_Impl->m_FdToPath.find(static_cast<int>(events[i].ident));
                if (it == m_Impl->m_FdToPath.end()) continue;

                std::lock_guard ql(m_QueueMutex);
                m_Queue.push_back(it->second);
            }
        }
    }

} // namespace Echelon

#endif // ECHELON_PLATFORM_MACOS
