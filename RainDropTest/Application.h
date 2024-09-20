#pragma once

#include "stdafx.h"
#include "DXApp.h"

class Win32Application
{
public:
    //static int Run(app::dx_app* p_app, HINSTANCE hInstance, int nCmdShow);
    static HWND GetHandleWindow() { return g_handler_window; }
protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static HWND g_handler_window;
};

