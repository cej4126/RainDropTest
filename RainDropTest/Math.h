#pragma once
#include "stdafx.h"

namespace math {


    // Align by rounding down. Will result in a multiple of 'alignment' that is less than or equal to 'size'.
    template<UINT64 alignment>
    [[nodiscard]] constexpr UINT64 align_size_down(UINT64 size)
    {
        static_assert(alignment, "Alignment must be non-zero.");
        constexpr UINT64 mask{ alignment - 1 };
        static_assert(!(alignment & mask), "Alignment should be a power of 2.");
        return (size & ~mask);
    }


    // Align by rounding up. Will result in a multiple of 'alignment' that is greater than or equal to 'size'.
    template<UINT64 alignment>
    [[nodiscard]] constexpr UINT64 align_size_up(UINT64 size)
    {
        static_assert(alignment, "Alignment must be non-zero.");
        constexpr UINT64 mask{ alignment - 1 };
        static_assert(!(alignment & mask), "Alignment should be a power of 2.");
        return ((size + mask) & ~mask);
    }

    [[nodiscard]] constexpr UINT64 calc_crc32_u64(const UINT8* const data, UINT64 size)
    {
        assert(size >= sizeof(UINT64));
        UINT64 crc{ 0 };
        const UINT8* at{ data };
        const UINT8* const end{ data + align_size_down<sizeof(UINT64)>(size) };
        while (at < end)
        {
            crc = _mm_crc32_u64(crc, *((const UINT64*)at));
            at += sizeof(UINT64);
        }

        return crc;
    }
}