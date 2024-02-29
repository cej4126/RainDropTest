
#include "stdafx.h"
#include "Win32Application.h"
#include "StepTimer.h"

time_it timer{};

HWND Win32Application::m_hander_window = nullptr;

int Win32Application::Run(DXSample* pSample, HINSTANCE hInstance, int m_cmd_show)
{
    // Parse the command line parameters
    int argc{ 0 };
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    pSample->ParseCommandLineArgs(argv, argc);
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

    RECT window_rect = { 0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight()) };

    // CreateWindowW(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    m_hander_window = CreateWindow(window_class.lpszClassName, pSample->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr, nullptr,
        hInstance,
        pSample);

    // Initialize the sample. OnInit is defined in each child-implementation of DXSample.
    pSample->OnInit();

    ShowWindow(m_hander_window, m_cmd_show);

    // Main sample loop.

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any message in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    pSample->OnDestroy();

    return static_cast<char>(msg.wParam);
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    DXSample* pSample = reinterpret_cast<DXSample*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the DXSample* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_KEYDOWN:
        if (pSample)
        {
            pSample->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pSample)
        {
            pSample->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_PAINT:
        timer.begin();
        if (pSample)
        {
            float dt{ timer.dt_avg() };
            pSample->OnUpdate(dt);
            pSample->OnRender();
        }
        timer.end();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}

