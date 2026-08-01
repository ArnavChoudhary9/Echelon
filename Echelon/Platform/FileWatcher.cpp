#include "Platform/FileWatcher.hpp"

// Poll() is identical on all platforms — it drains the queue that the
// background thread (defined in the platform-specific backend .cpp) fills.

namespace Echelon {

    std::vector<fs::path> FileWatcher::Poll() {
        std::vector<fs::path> out;
        std::lock_guard lk(m_QueueMutex);
        out.swap(m_Queue);
        return out;
    }

} // namespace Echelon
