#pragma once

/**
 * @file FileWatcher.hpp
 * @brief OS-native file-change watcher with a cross-platform public API.
 *
 * Platform dispatch (selected at compile time via removefiles in premake):
 *   Linux   → inotify              (Platform/Backends/Linux/LinuxFileWatcher.cpp)
 *   Windows → ReadDirectoryChangesW(Platform/Backends/Windows/WindowsFileWatcher.cpp)
 *   macOS   → kqueue               (Platform/Backends/MacOS/MacOSFileWatcher.cpp)
 *   other   → 250 ms background poll (Platform/Backends/Generic/GenericFileWatcher.cpp)
 *
 * All platform-specific types are hidden behind FileWatcher::Impl (pimpl) so
 * this header pulls in only standard library headers — no Windows.h, inotify.h,
 * or sys/event.h leaks into every translation unit that includes it.
 */

#include "Core/Base.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Echelon {

    class FileWatcher {
    public:
        FileWatcher();
        ~FileWatcher();

        FileWatcher(const FileWatcher&)            = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;

        /** @brief Spawn the background watcher thread. Call once before Watch(). */
        void Start();

        /** @brief Stop the background thread and release all OS resources. */
        void Stop();

        /** @brief Begin watching a file for modifications. Main thread only. */
        void Watch(const fs::path& filePath);

        /** @brief Stop watching a file. Main thread only. */
        void Unwatch(const fs::path& filePath);

        /**
         * @brief Drain pending change events.
         * @return Paths modified since the last Poll().
         *         Cheap to call every frame — O(changed files), never blocks.
         */
        std::vector<fs::path> Poll();

    private:
        /** @brief Entry point for the background watcher thread. */
        void ThreadFunc();

        // ---- Cross-platform state (accessible from all TUs via member-function access) ----

        std::atomic<bool>     m_Running{ false };
        std::thread           m_Thread;
        std::mutex            m_QueueMutex;
        std::vector<fs::path> m_Queue;

        // ---- Platform-specific state, fully hidden from this header ----

        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };

} // namespace Echelon
