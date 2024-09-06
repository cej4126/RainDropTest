
#include "stdafx.h"
#include "Win32Application.h"
#include "Input.h"

HWND Win32Application::g_handler_window = nullptr;

int Win32Application::Run(dx_app* p_app, HINSTANCE hInstance, int m_cmd_show)
{
    // Parse the command line parameters
    int argc{ 0 };
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    p_app->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    // Initialize the window class
    WNDCLASSEX window_class = { 0 };
    window_class.cbSize = sizeof(WNDCLASSEX);           // UINT        cbSize;
    window_class.style = CS_HREDRAW | CS_VREDRAW;       // UINT        style;
    window_class.lpfnWndProc = WindowProc;              // WNDPROC     lpfnWndProc;
    window_class.hInstance = hInstance;                 // HINSTANCE   hInstance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW); // HCURSOR     hCursor;
    window_class.lpszClassName = L"Rain Test";          // LPCWSTR     lpszClassName;
    window_class.cbClsExtra = 0;                        // int         cbClsExtra;
    window_class.cbWndExtra = 0;                        // int         cbWndExtra;
    window_class.hIcon = 0;                             // HICON       hIcon;
    window_class.hbrBackground = 0;                     // HBRUSH      hbrBackground;
    window_class.lpszMenuName = 0;                      // LPCWSTR     lpszMenuName;
    window_class.hIconSm = 0;                           // HICON       hIconSm;
    RegisterClassEx(&window_class);

    RECT window_rect = { 0, 0, static_cast<LONG>(p_app->GetWidth()), static_cast<LONG>(p_app->GetHeight()) };

    // CreateWindowW(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    g_handler_window = CreateWindow(window_class.lpszClassName, p_app->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr, nullptr,
        hInstance,
        p_app);

    // Initialize the sample. OnInit is defined in each child-implementation of dx_app.
    p_app->OnInit(p_app->GetWidth(), p_app->GetHeight());

    p_app->create_surface(g_handler_window, p_app->GetWidth(), p_app->GetHeight());

    ShowWindow(g_handler_window, m_cmd_show);

    // Main sample loop.

    MSG msg = {};
    bool is_running{ true };
    while (is_running)
    {
        // Process any message in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            is_running &= (msg.message != WM_QUIT);
        }
        p_app->run();
    }

    p_app->OnDestroy();

    return static_cast<char>(msg.wParam);
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    dx_app* p_app = reinterpret_cast<dx_app*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the dx_app* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    input::process_input_message(hWnd, message, wParam, lParam);

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}

