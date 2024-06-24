#pragma once
#include "stdafx.h"

namespace d3d12::math {

    // Align by rounding up. Will result in a multiple of 'alignment' that is greater than or equal to 'size'.
    template<UINT64 alignment>
    [[nodiscard]] constexpr UINT64
        align_size_up(UINT64 size)
    {
        static_assert(alignment, "Alignment must be non-zero.");
        constexpr UINT64 mask{ alignment - 1 };
        static_assert(!(alignment & mask), "Alignment should be a power of 2.");
        return ((size + mask) & ~mask);
    }
}