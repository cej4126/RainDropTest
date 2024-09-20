#pragma once
#include "stdafx.h"

namespace windows {

    using window_proc = LRESULT(*)(HWND, UINT, WPARAM, LPARAM);
    struct window_init_info
    {
        window_proc callback { nullptr };
        HWND parent{ nullptr };
        const wchar_t* caption{ nullptr };
        INT32 left{ 0 };
        INT32 right{ 0 };
        INT32 width{ 1920 };
        INT32 height{ 1080 };
    };

    class window
    {
    public:
        constexpr explicit window(UINT id) : _id{ id } {};
        constexpr window() = default;
        constexpr UINT get_id() const { return _id; }
        constexpr bool is_valid() const { return _id != Invalid_Index; }
        
    private:
        UINT _id{ Invalid_Index };
    };

    window create(const window_init_info* init_info);
}
