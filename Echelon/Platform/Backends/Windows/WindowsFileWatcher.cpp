// Safety guard: this file must only be compiled on Windows.
// Premake's removefiles already ensures this; the guard is a belt-and-suspenders fallback.
#if defined(ECHELON_PLATFORM_WINDOWS)

#include "Platform/FileWatcher.hpp"
#include "Core/Log.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <string>

namespace Echelon {

    struct FileWatcher::Impl {
        HANDLE m_StopEvent = nullptr;

        mutable std::shared_mutex m_WatchMutex;

        struct DirWatch {
            HANDLE     handle    = INVALID_HANDLE_VALUE;
            OVERLAPPED overlapped{};
            alignas(DWORD) char buffer[65536]{};
            int refCount = 0;
        };

        std::unordered_map<std::string, DirWatch> m_DirWatches;
    };

    FileWatcher::FileWatcher()  : m_Impl(std::make_unique<Impl>()) {}
    FileWatcher::~FileWatcher() = default;

    void FileWatcher::Start() {
        m_Impl->m_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_Impl->m_StopEvent) {
            ECHELON_LOG_ERROR("[FileWatcher] CreateEvent failed: {}", GetLastError());
            return;
        }
        m_Running.store(true);
        m_Thread = std::thread(&FileWatcher::ThreadFunc, this);
    }

    void FileWatcher::Stop() {
        m_Running.store(false);
        if (m_Impl->m_StopEvent)
            SetEvent(m_Impl->m_StopEvent);
        if (m_Thread.joinable())
            m_Thread.join();

        std::unique_lock lk(m_Impl->m_WatchMutex);
        for (auto& [dir, dw] : m_Impl->m_DirWatches) {
            if (dw.overlapped.hEvent)
                CloseHandle(dw.overlapped.hEvent);
            if (dw.handle != INVALID_HANDLE_VALUE)
                CloseHandle(dw.handle);
        }
        m_Impl->m_DirWatches.clear();

        if (m_Impl->m_StopEvent) {
            CloseHandle(m_Impl->m_StopEvent);
            m_Impl->m_StopEvent = nullptr;
        }
    }

    void FileWatcher::Watch(const fs::path& filePath) {
        const std::string dir = filePath.parent_path().string();

        std::unique_lock lk(m_Impl->m_WatchMutex);

        auto& dw = m_Impl->m_DirWatches[dir];
        if (dw.handle == INVALID_HANDLE_VALUE || dw.handle == nullptr) {
            std::wstring wdir(dir.begin(), dir.end());
            dw.handle = CreateFileW(wdir.c_str(),
                                    FILE_LIST_DIRECTORY,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                    nullptr);
            if (dw.handle == INVALID_HANDLE_VALUE) {
                ECHELON_LOG_WARN("[FileWatcher] CreateFile failed for '{}': {}", dir, GetLastError());
                m_Impl->m_DirWatches.erase(dir);
                return;
            }
            ZeroMemory(&dw.overlapped, sizeof(dw.overlapped));
            dw.overlapped.hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            dw.refCount = 0;
        }
        dw.refCount++;
    }

    void FileWatcher::Unwatch(const fs::path& filePath) {
        const std::string dir = filePath.parent_path().string();

        std::unique_lock lk(m_Impl->m_WatchMutex);

        auto it = m_Impl->m_DirWatches.find(dir);
        if (it == m_Impl->m_DirWatches.end()) return;

        if (--it->second.refCount <= 0) {
            if (it->second.overlapped.hEvent)
                CloseHandle(it->second.overlapped.hEvent);
            CloseHandle(it->second.handle);
            m_Impl->m_DirWatches.erase(it);
        }
    }

    void FileWatcher::ThreadFunc() {
        while (m_Running.load()) {
            // Snapshot dir list and issue ReadDirectoryChangesW for each
            std::vector<std::pair<std::string, HANDLE>> dirs;
            {
                std::shared_lock rl(m_Impl->m_WatchMutex);
                for (auto& [dir, dw] : m_Impl->m_DirWatches) {
                    dirs.emplace_back(dir, dw.overlapped.hEvent);
                    ReadDirectoryChangesW(dw.handle,
                                         dw.buffer, sizeof(dw.buffer),
                                         FALSE,
                                         FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
                                         nullptr, &dw.overlapped, nullptr);
                }
            }

            if (dirs.empty()) {
                WaitForSingleObject(m_Impl->m_StopEvent, 200);
                continue;
            }

            // Build handle array: per-dir completion events + stop event
            std::vector<HANDLE> handles;
            handles.reserve(dirs.size() + 1);
            for (auto& [dir, ev] : dirs) handles.push_back(ev);
            handles.push_back(m_Impl->m_StopEvent);

            DWORD result = WaitForMultipleObjects(
                static_cast<DWORD>(handles.size()), handles.data(), FALSE, 200);

            if (result == WAIT_TIMEOUT || result == WAIT_FAILED) continue;

            DWORD idx = result - WAIT_OBJECT_0;
            if (idx >= static_cast<DWORD>(dirs.size())) continue; // stop event

            const std::string& changedDir = dirs[idx].first;

            DWORD transferred = 0;
            std::shared_lock rl(m_Impl->m_WatchMutex);
            auto it = m_Impl->m_DirWatches.find(changedDir);
            if (it == m_Impl->m_DirWatches.end()) continue;

            GetOverlappedResult(it->second.handle, &it->second.overlapped,
                                &transferred, FALSE);
            if (transferred == 0) continue;

            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(it->second.buffer);
            while (true) {
                if (info->Action == FILE_ACTION_MODIFIED ||
                    info->Action == FILE_ACTION_RENAMED_NEW_NAME) {

                    int wlen   = static_cast<int>(info->FileNameLength / sizeof(WCHAR));
                    int needed = WideCharToMultiByte(CP_UTF8, 0,
                                                     info->FileName, wlen,
                                                     nullptr, 0, nullptr, nullptr);
                    std::string filename(needed, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, info->FileName, wlen,
                                        filename.data(), needed, nullptr, nullptr);

                    std::lock_guard ql(m_QueueMutex);
                    m_Queue.push_back(fs::path(changedDir) / filename);
                }

                if (info->NextEntryOffset == 0) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<char*>(info) + info->NextEntryOffset);
            }
        }
    }

} // namespace Echelon

#endif // ECHELON_PLATFORM_WINDOWS
