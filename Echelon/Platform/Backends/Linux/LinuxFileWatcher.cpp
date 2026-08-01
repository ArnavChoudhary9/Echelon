// Safety guard: this file must only be compiled on Linux.
// Premake's removefiles already ensures this; the guard is a belt-and-suspenders fallback.
#if defined(ECHELON_PLATFORM_LINUX)

#include "Platform/FileWatcher.hpp"
#include "Core/Log.hpp"

#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <shared_mutex>
#include <unordered_map>

namespace Echelon {

    struct FileWatcher::Impl {
        int m_InotifyFd = -1;

        mutable std::shared_mutex m_WatchMutex;

        // inotify watch descriptor → parent directory path
        std::unordered_map<int, fs::path>    m_WdToDir;
        // directory string → inotify watch descriptor
        std::unordered_map<std::string, int> m_DirToWd;
        // directory string → number of watched files inside it
        std::unordered_map<std::string, int> m_DirRefCount;
    };

    FileWatcher::FileWatcher()  : m_Impl(std::make_unique<Impl>()) {}
    FileWatcher::~FileWatcher() = default;

    void FileWatcher::Start() {
        m_Impl->m_InotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (m_Impl->m_InotifyFd < 0) {
            ECHELON_LOG_ERROR("[FileWatcher] inotify_init1 failed: {}", std::strerror(errno));
            return;
        }
        m_Running.store(true);
        m_Thread = std::thread(&FileWatcher::ThreadFunc, this);
    }

    void FileWatcher::Stop() {
        m_Running.store(false);
        if (m_Thread.joinable())
            m_Thread.join();

        if (m_Impl->m_InotifyFd >= 0) {
            close(m_Impl->m_InotifyFd);
            m_Impl->m_InotifyFd = -1;
        }

        std::unique_lock lk(m_Impl->m_WatchMutex);
        m_Impl->m_WdToDir.clear();
        m_Impl->m_DirToWd.clear();
        m_Impl->m_DirRefCount.clear();
    }

    void FileWatcher::Watch(const fs::path& filePath) {
        if (m_Impl->m_InotifyFd < 0) return;

        const std::string dir = filePath.parent_path().string();

        std::unique_lock lk(m_Impl->m_WatchMutex);

        if (m_Impl->m_DirToWd.find(dir) == m_Impl->m_DirToWd.end()) {
            // Watch the directory — editors often write via a temp file + rename,
            // so IN_MOVED_TO catches those atomic saves as well as IN_CLOSE_WRITE.
            int wd = inotify_add_watch(m_Impl->m_InotifyFd, dir.c_str(),
                                       IN_CLOSE_WRITE | IN_MOVED_TO);
            if (wd < 0) {
                ECHELON_LOG_WARN("[FileWatcher] inotify_add_watch failed for '{}': {}",
                                 dir, std::strerror(errno));
                return;
            }
            m_Impl->m_WdToDir[wd]      = filePath.parent_path();
            m_Impl->m_DirToWd[dir]     = wd;
            m_Impl->m_DirRefCount[dir] = 0;
        }
        m_Impl->m_DirRefCount[dir]++;
    }

    void FileWatcher::Unwatch(const fs::path& filePath) {
        if (m_Impl->m_InotifyFd < 0) return;

        const std::string dir = filePath.parent_path().string();

        std::unique_lock lk(m_Impl->m_WatchMutex);

        auto rcIt = m_Impl->m_DirRefCount.find(dir);
        if (rcIt == m_Impl->m_DirRefCount.end()) return;

        if (--rcIt->second <= 0) {
            int wd = m_Impl->m_DirToWd[dir];
            inotify_rm_watch(m_Impl->m_InotifyFd, wd);
            m_Impl->m_WdToDir.erase(wd);
            m_Impl->m_DirToWd.erase(dir);
            m_Impl->m_DirRefCount.erase(rcIt);
        }
    }

    void FileWatcher::ThreadFunc() {
        constexpr size_t BUF_LEN = 10 * (sizeof(inotify_event) + NAME_MAX + 1);
        alignas(inotify_event) char buf[BUF_LEN];

        while (m_Running.load()) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(m_Impl->m_InotifyFd, &rfds);

            timeval tv{ 0, 200'000 }; // 200 ms timeout so Stop() wakes within ~200 ms
            int ret = select(m_Impl->m_InotifyFd + 1, &rfds, nullptr, nullptr, &tv);
            if (ret <= 0) continue;

            ssize_t len = read(m_Impl->m_InotifyFd, buf, BUF_LEN);
            if (len <= 0) continue;

            std::shared_lock rl(m_Impl->m_WatchMutex);

            for (char* p = buf; p < buf + len; ) {
                auto* ev = reinterpret_cast<inotify_event*>(p);
                p += sizeof(inotify_event) + ev->len;

                if (ev->len == 0 || ev->name[0] == '\0') continue;
                if (!(ev->mask & (IN_CLOSE_WRITE | IN_MOVED_TO))) continue;

                auto it = m_Impl->m_WdToDir.find(ev->wd);
                if (it == m_Impl->m_WdToDir.end()) continue;

                fs::path changed = it->second / ev->name;

                std::lock_guard ql(m_QueueMutex);
                m_Queue.push_back(std::move(changed));
            }
        }
    }

} // namespace Echelon

#endif // ECHELON_PLATFORM_LINUX
