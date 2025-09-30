#pragma once

namespace Echelon {
    /**
     * @brief Creates a bitmask with a 1 at the specified position.
     * 
     * @param x The position of the bit to set (0-indexed).
     * @return int The bitmask with the bit at position x set to 1.
     */
    constexpr int BIT(int x) { return 1 << x; }
}
