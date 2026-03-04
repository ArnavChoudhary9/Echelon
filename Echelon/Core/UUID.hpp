#pragma once

#include <cstdint>

namespace Echelon {
    class UUID {
    public:
        UUID();
        UUID(uint64_t);
        UUID(const UUID&) = default;

        UUID& operator=(const UUID& other) = default;
        
        ~UUID() = default;

        operator uint64_t() const { return m_UUID; }

    private:
        uint64_t m_UUID;
    };
}
