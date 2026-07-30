#pragma once

/**
 * @file ImportResult.hpp
 * @brief Result of an import attempt: success flag, message, and the produced asset.
 */

#include "Core/Base.hpp"
#include "Asset/Asset.hpp"

#include <string>
#include <memory>

namespace Echelon {

    class ImportResult {
    public:
        ImportResult() = default;

        /** @brief Failure result carrying a diagnostic message. */
        ImportResult(const std::string& message)
            : m_Message(message), m_Success(false) {}

        /** @brief Success result carrying the produced asset (Ref<T> upcasts to Ref<Asset>). */
        ImportResult(const Ref<Asset>& asset)
            : m_Message("Import successful"), m_Success(true), m_Asset(asset) {}

        ImportResult(const ImportResult&) = default;
        ImportResult(ImportResult&&) = default;
        ImportResult& operator=(const ImportResult&) = default;
        ImportResult& operator=(ImportResult&&) = default;

        const std::string& GetMessage() const { return m_Message; }
        bool IsSuccess() const { return m_Success; }

        Ref<Asset> GetAsset() const { return m_Asset; }
        template<class T> Ref<T> GetAs() const { return std::dynamic_pointer_cast<T>(m_Asset); }

    private:
        std::string m_Message;
        bool        m_Success = false;
        Ref<Asset>  m_Asset   = nullptr;
    };

} // namespace Echelon
