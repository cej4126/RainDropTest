#pragma once
#include "stdafx.h"

namespace windows {

    using window_proc = LRESULT(*)(HWND, UINT, WPARAM, LPARAM);
    struct window_init_info
    {
        window_proc callback { nullptr };
        HWND parent_handle{ nullptr };
        const wchar_t* caption{ nullptr };
        INT32 left{ 0 };
        INT32 top{ 0 };
        INT32 width{ 1920 };
        INT32 height{ 1080 };
    };

    class window
    {
    public:
        constexpr explicit window(UINT id) : m_id{ id } {};
        constexpr window() = default;
        constexpr UINT get_id() const { return m_id; }
        constexpr bool is_valid() const { return m_id != Invalid_Index; }
        
        void* handle() const;
        UINT width() const;
        UINT height() const;

    private:
        UINT m_id{ Invalid_Index };
    };

    window create(const window_init_info* init_info);
    void remove(UINT id);

    HWND get_window_handle(UINT id);
    DirectX::XMUINT4 get_window_size(UINT id);
}
