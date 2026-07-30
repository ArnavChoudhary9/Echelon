#pragma once

#include "Core/Base.hpp"

namespace Echelon {

    class ImportContext {
    public:
        ImportContext() = default;
        ImportContext(const fs::path& path)
            : m_Path(path) {}
        ImportContext(const ImportContext&) = default;
        ImportContext(ImportContext&&) = default;

        ImportContext& operator=(const ImportContext&) = default;
        ImportContext& operator=(ImportContext&&) = default;

        const fs::path& GetPath() const { return m_Path; }
        fs::path GetAbsolutePath() const { return fs::absolute(m_Path); }

        std::string GetPathString() const { return m_Path.string(); }

    private:
        fs::path m_Path;
    };

} // namespace Echelon
