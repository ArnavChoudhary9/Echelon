#pragma once

#include <cstdint>
#include <functional>
#include <uuid.h>

namespace Echelon {
    class UUID {
    public:
        UUID();
        UUID(const uuids::uuid&);
        UUID(const std::string&);

        UUID(UUID&&) = default;
        UUID(const UUID&) = default;

        static UUID FromName(const std::string&);

        UUID& operator=(const UUID& other) = default;

        ~UUID() = default;

        [[nodiscard]] const uuids::uuid& GetUUID() const { return m_UUID; }
        [[nodiscard]] std::string ToString() const { return uuids::to_string(m_UUID); }
        [[nodiscard]] operator std::string() const { return ToString(); }
        [[nodiscard]] size_t Hash() const { return std::hash<uuids::uuid>{}(m_UUID); }

        bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
        bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }

    private:
        uuids::uuid m_UUID;
    };
}

namespace std {
    template<>
    struct hash<Echelon::UUID> {
        size_t operator()(const Echelon::UUID& uuid) const {
            return uuid.Hash();
        }
    };
}
