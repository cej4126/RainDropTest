#include "Window.h"
#include "FreeList.h"
#include "Input.h"

namespace windows {

    namespace {
        struct window_info
        {
            HWND hwnd{ nullptr };
            RECT client_area{ 0, 0, 1920, 1080 };
            RECT    fullscreen_area{};
            POINT   top_left{ 0, 0 };
            DWORD   style{ WS_VISIBLE };
            bool    is_fullscreen{ false };
            bool    is_closed{ false };
        };

        utl::free_list<window_info> windows;
        bool resized{ false };

        window_info& get_from_handle(HWND handle)
        {
            const UINT id{ (UINT)GetWindowLongPtr(handle, GWLP_USERDATA) };
            return windows[id];
        }

        LRESULT CALLBACK internal_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            switch (msg)
            {
            case WM_NCCREATE:
            {
                // Put the window id in the user data field of window's data buffer.
                DEBUG_OP(SetLastError(0));
                const UINT id{ windows.add() };
                windows[id].hwnd = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)id);
                assert(GetLastError() == 0);
            }
            break;
            case WM_DESTROY:
                get_from_handle(hwnd).is_closed = true;
                break;
            case WM_SIZE:
                resized = (wparam != SIZE_MINIMIZED);
                break;

            default:
                break;
            }

            input::process_input_message(hwnd, msg, wparam, lparam);

            if (resized && GetKeyState(VK_LBUTTON) >= 0)
            {
                window_info& info{ get_from_handle(hwnd) };
                assert(info.hwnd);
                GetClientRect(info.hwnd, info.is_fullscreen ? &info.fullscreen_area : &info.client_area);
                resized = false;
            }

            if (msg == WM_SYSCOMMAND && wparam == SC_KEYMENU)
            {
                return 0;
            }

            LONG_PTR long_ptr{ GetWindowLongPtr(hwnd, 0) };
            return long_ptr ? ((window_proc)long_ptr)(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
        }
    }


    void* window::handle() const
    {
        return get_window_handle(m_id);
    }

    UINT window::width() const
    {
        DirectX::XMUINT4 s{ get_window_size(m_id) };
        return s.z - s.x;
    }

    UINT window::height() const
    {
        DirectX::XMUINT4 s{ get_window_size(m_id) };
        return s.w - s.y;
    }

    window create(const window_init_info* init_info)
    {
        assert(init_info->callback);
        window_proc callback{ init_info->callback };
        HWND parent_handle{ nullptr };

        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(WNDCLASSEX);                       // UINT        cbSize;
        wc.style = CS_HREDRAW | CS_VREDRAW;                   // UINT        style;
        wc.lpfnWndProc = internal_window_proc;                // WNDPROC     lpfnWndProc;
        wc.cbClsExtra = 0;                                    // int         cbClsExtra;
        wc.cbWndExtra = sizeof(callback);                     // int         cbWndExtra;
        wc.hInstance = 0;                                     // HINSTANCE   hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);           // HICON       hIcon;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);             // HCURSOR     hCursor;
        wc.hbrBackground = CreateSolidBrush(RGB(26, 48, 76)); // HBRUSH      hbrBackground;
        wc.lpszMenuName = NULL;                               // LPCWSTR     lpszMenuName;
        wc.lpszClassName = L"RainDrop";                       // LPCWSTR     lpszClassName;
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);         // HICON       hIconSm;
        RegisterClassEx(&wc);

        window_info info{};
        info.client_area.right = (init_info && init_info->width) ? info.client_area.left + init_info->width : info.client_area.right;
        info.client_area.bottom = (init_info && init_info->height) ? info.client_area.top + init_info->height : info.client_area.bottom;
        info.style |= WS_OVERLAPPEDWINDOW;

        RECT rect{ info.client_area };

        // adjust the window size for correct device size
        AdjustWindowRect(&rect, info.style, FALSE);

        const INT left{ init_info->left };
        const INT top{ init_info->top };
        const INT width{ rect.right - rect.left };
        const INT height{ rect.bottom - rect.top };

        info.hwnd = CreateWindowEx(
            _In_ 0,
            _In_opt_ wc.lpszClassName,
            _In_opt_ init_info->caption,
            _In_ info.style,
            _In_ left, _In_ top,
            _In_ width, _In_ height,
            _In_opt_ parent_handle,
            _In_opt_ NULL,
            _In_opt_ NULL,
            _In_opt_ NULL);

        if (info.hwnd)
        {
            DEBUG_OP(SetLastError(0));
            SetWindowLongPtr(info.hwnd, 0, (LONG_PTR)callback);
            assert(GetLastError() == 0);
            ShowWindow(info.hwnd, SW_SHOWNORMAL);
            UpdateWindow(info.hwnd);

            UINT id{ (UINT)GetWindowLongPtr(info.hwnd, GWLP_USERDATA) };
            windows[id] = info;

            return window{ id };
        }
        return {};
    }

    void remove(UINT id)
    {
        DestroyWindow(windows[id].hwnd);
        windows.remove(id);
    }

    HWND get_window_handle(UINT id)
    {
        return windows[id].hwnd;
    }

    DirectX::XMUINT4 get_window_size(UINT id)
    {
        window_info& info{ windows[id] };
        RECT& area{ info.is_fullscreen ? info.fullscreen_area : info.client_area };
        return { (UINT)area.left, (UINT)area.top, (UINT)area.right, (UINT)area.bottom };
    }
}