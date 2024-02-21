#include "stdafx.h"
#include "D3D12RainDrop.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    D3D12RainDrop rain(1080, 800, L"D3D12 rain");
    return Win32Application::Run(&rain, hInstance, nCmdShow);
}